#include <SDL3/SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "pacman_alpm.h"
#include <alpm.h>

static pacman_t alpm_backend;
static pacman_t *alpm_self(void);
static alpm_handle_t *alpm_handle=NULL;
/* 外部 pacman 事务不会更新本进程的 libalpm 本地数据库缓存；每次重新
 * 枚举前重建 handle，避免删除后还从缓存里读到旧包。递归 resync 期间
 * 不能重开 handle，否则外层遍历的 alpm_pkg_t 会失效。 */
static int alpm_listing_depth=0;
/* pacman -Qm 的缓存，用于查询未出现在 installed 页的依赖包是否来自 AUR。 */
static char **alpm_foreign_names=NULL;
static size_t alpm_foreign_name_count=0;
static int alpm_foreign_unknown=0;

static char *alpm_strdup(const char *text){
    if(text==NULL){
        return NULL;
    }
    size_t length=strlen(text)+1;
    char *copy=malloc(length);
    if(copy!=NULL){
        memcpy(copy,text,length);
    }
    return copy;
}

/* 把 alpm 的许可证链表复制成以 NULL 结尾的字符串数组。
 * 没有许可证或分配失败时返回 NULL。 */
static char **alpm_copy_licenses(alpm_list_t *licenses){
    if(licenses==NULL){
        return NULL;
    }

    size_t count=0;
    for(alpm_list_t *i=licenses;i!=NULL;i=i->next){
        const char *text=(const char *)i->data;
        if(text!=NULL&&text[0]!='\0'){
            count++;
        }
    }
    if(count==0){
        return NULL;
    }

    char **result=calloc(count+1,sizeof(*result));
    if(result==NULL){
        return NULL;
    }

    size_t out=0;
    for(alpm_list_t *i=licenses;i!=NULL;i=i->next){
        const char *text=(const char *)i->data;
        if(text==NULL||text[0]=='\0'){
            continue;
        }
        result[out]=alpm_strdup(text);
        if(result[out]==NULL){
            for(size_t j=0;j<out;j++){
                free(result[j]);
            }
            free(result);
            return NULL;
        }
        out++;
    }
    result[out]=NULL;
    return result;
}

static void alpm_free_licenses(char **licenses){
    if(licenses==NULL){
        return;
    }
    for(char **i=licenses;*i!=NULL;i++){
        free(*i);
    }
    free(licenses);
}

int pacman_alpm_available(void){
    return access("/var/lib/pacman/local",R_OK)==0;
}

/* 打开本地数据库句柄；重复调用直接复用。pacman_quit_all() 负责释放。 */
static int alpm_open(void){
    if(alpm_handle!=NULL){
        return 1;
    }

    alpm_errno_t error=ALPM_ERR_OK;
    alpm_handle=alpm_initialize("/","/var/lib/pacman/",&error);
    if(alpm_handle==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "alpm: initialize failed: %s",alpm_strerror(error));
        return 0;
    }
    return 1;
}

/* 丢弃 libalpm 的本地数据库缓存，重新打开 handle。 */
static int alpm_reopen(void){
    if(alpm_handle!=NULL){
        alpm_release(alpm_handle);
        alpm_handle=NULL;
    }
    return alpm_open();
}

/* 读取一个 alpm 包并交给全局注册表，字段所有权同时转交 pkg_add()。
 * 只列出用户显式安装的包；作为依赖自动安装的包不进入 UI。 */
static int alpm_append(alpm_pkg_t *source){
    if(alpm_pkg_get_reason(source)!=ALPM_PKG_REASON_EXPLICIT){
        return 1;
    }

    package_t pkg;
    memset(&pkg,0,sizeof(pkg));

    pkg.from=alpm_self();
    pkg.meta_data="local";
    pkg.name=alpm_strdup(alpm_pkg_get_name(source));
    pkg.version=alpm_strdup(alpm_pkg_get_version(source));
    pkg.description=alpm_strdup(alpm_pkg_get_desc(source));
    pkg.licenses=alpm_copy_licenses(alpm_pkg_get_licenses(source));
    pkg.submitter=alpm_strdup(alpm_pkg_get_packager(source));
    pkg.stat=PKG_STAT_INSTALLED;
    pkg.progress=0.0;

    off_t installed_size=alpm_pkg_get_isize(source);
    pkg.size=installed_size>=0?(unsigned long)installed_size:(unsigned long)-1;

    if(pkg.name==NULL||pkg.version==NULL){
        free(pkg.name);
        free(pkg.version);
        free(pkg.description);
        alpm_free_licenses(pkg.licenses);
        free(pkg.submitter);
        return 0;
    }

    pkg_add(&pkg);
    return 1;
}

/* ---- pacman CLI：官方仓库搜索、更新检查和包操作 ----
 * 本地数据库继续走 libalpm；同步数据库和事务交给 pacman 自己处理，
 * 这样不需要在 GUI 里重复解析 pacman.conf，也能沿用它的镜像/签名配置。 */

#define ALPM_OUTPUT_LIMIT (1024*1024)

static const char *alpm_find_tool(const char *name){
    if(strcmp(name,"pacman")==0){
        const char *override=getenv("PACMAN_GUI_PACMAN");
        if(override!=NULL&&override[0]!='\0'){
            return override;
        }
    }

    static const char *paths[][4]={
        {"pacman","/usr/bin/pacman","/bin/pacman",NULL},
        {"pkexec","/usr/bin/pkexec","/usr/sbin/pkexec","/bin/pkexec"},
        {"sudo","/usr/bin/sudo","/bin/sudo",NULL}
    };
    for(size_t i=0;i<sizeof(paths)/sizeof(paths[0]);i++){
        if(strcmp(paths[i][0],name)!=0){
            continue;
        }
        for(size_t j=1;j<sizeof(paths[i])/sizeof(paths[i][0]);j++){
            if(paths[i][j]!=NULL&&access(paths[i][j],X_OK)==0){
                return paths[i][j];
            }
        }
        return NULL;
    }
    return NULL;
}

/* 运行命令并可选捕获 stdout/stderr；返回退出码，-1 表示 fork/exec 失败。
 * 不经过 shell，包名和关键词都作为独立参数传入。 */
static int alpm_run(char *const argv[],char **output){
    if(output!=NULL){
        *output=NULL;
    }
    int fds[2];
    if(pipe(fds)!=0){
        return -1;
    }

    pid_t pid=fork();
    if(pid<0){
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if(pid==0){
        close(fds[0]);
        if(output!=NULL){
            dup2(fds[1],STDOUT_FILENO);
            dup2(fds[1],STDERR_FILENO);
        }
        close(fds[1]);
        execvp(argv[0],argv);
        _exit(127);
    }

    close(fds[1]);
    char *buffer=NULL;
    size_t used=0;
    size_t capacity=0;
    char chunk[4096];
    for(;;){
        ssize_t got=read(fds[0],chunk,sizeof(chunk));
        if(got<0&&errno==EINTR){
            continue;
        }
        if(got<=0){
            break;
        }
        if(output==NULL||used>ALPM_OUTPUT_LIMIT){
            continue;
        }
        size_t take=(size_t)got;
        if(take>ALPM_OUTPUT_LIMIT-used){
            take=ALPM_OUTPUT_LIMIT-used;
        }
        if(used+take+1>capacity){
            size_t next=capacity>0?capacity*2:4096;
            while(next<used+take+1){
                next*=2;
            }
            char *next_buffer=realloc(buffer,next);
            if(next_buffer==NULL){
                break;
            }
            buffer=next_buffer;
            capacity=next;
        }
        memcpy(buffer+used,chunk,take);
        used+=take;
    }
    close(fds[0]);

    int status=0;
    while(waitpid(pid,&status,0)<0&&errno==EINTR){
    }

    if(output!=NULL){
        if(buffer==NULL){
            buffer=malloc(1);
            if(buffer!=NULL){
                used=0;
            }
        }
        if(buffer!=NULL){
            buffer[used]='\0';
        }
        *output=buffer;
    }else{
        free(buffer);
    }

    if(WIFEXITED(status)){
        return WEXITSTATUS(status);
    }
    return -1;
}

static int alpm_name_valid(const char *name){
    if(name==NULL||name[0]=='\0'||name[0]=='-'){
        return 0;
    }
    for(size_t i=0;name[i]!='\0';i++){
        unsigned char c=(unsigned char)name[i];
        int ok=(c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||
               c=='@'||c=='.'||c=='_'||c=='+'||c=='-';
        if(!ok){
            return 0;
        }
    }
    return 1;
}

/* 由 pacman 自己输出“仓库/包名 版本”，这里只取包名和版本。 */
static void alpm_search(const char *keyword){
    if(keyword==NULL||keyword[0]=='\0'){
        return;
    }
    const char *pacman=alpm_find_tool("pacman");
    if(pacman==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"alpm: pacman binary not found");
        return;
    }

    char *argv[]={(char *)pacman,"-Ss","--color","never","--",(char *)keyword,NULL};
    char *text=NULL;
    int status=alpm_run(argv,&text);
    if(text==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"alpm: pacman -Ss failed (%d)",status);
        return;
    }

    char *cursor=text;
    while(cursor!=NULL&&*cursor!='\0'){
        char *line=cursor;
        char *newline=strchr(cursor,'\n');
        if(newline!=NULL){
            *newline='\0';
            cursor=newline+1;
        }else{
            cursor=NULL;
        }

        if(line[0]=='\0'||line[0]==' '||line[0]=='\t'||line[0]=='='||line[0]=='>'){
            continue;
        }
        char *space=strchr(line,' ');
        if(space==NULL){
            continue;
        }
        *space='\0';
        char *name=line;
        char *slash=strrchr(name,'/');
        if(slash!=NULL){
            name=slash+1;
        }
        char *version=space+1;
        while(*version==' '||*version=='\t'){
            version++;
        }
        char *version_end=version;
        while(*version_end!='\0'&&*version_end!=' '&&*version_end!='\t'){
            version_end++;
        }
        *version_end='\0';

        if(!alpm_name_valid(name)||version[0]=='\0'){
            continue;
        }

        package_t pkg;
        memset(&pkg,0,sizeof(pkg));
        pkg.from=alpm_self();
        pkg.name=alpm_strdup(name);
        pkg.version=alpm_strdup(version);
        pkg.stat=PKG_STAT_NULL;
        pkg.size=(unsigned long)-1;
        if(pkg.name==NULL||pkg.version==NULL){
            free(pkg.name);
            free(pkg.version);
            continue;
        }
        pkg_available_add(&pkg);
    }
    free(text);
}

static void alpm_clear_updates(void){
    pacman_t *backend=alpm_self();
    size_t count=pacman_package_count();
    for(size_t i=0;i<count;i++){
        package_t *pkg=pacman_package_at(i);
        if(pkg==NULL||pkg->from!=backend){
            continue;
        }
        free(pkg->new_version);
        pkg->new_version=NULL;
        pkg->update_available=0;
    }
}

/* 用 pacman -Qu 取可升级包。同步数据库的刷新仍由 pacman -Sy 负责，
 * 这里只读当前同步数据库，不需要 root。 */
static int alpm_check_updates(void){
    alpm_clear_updates();

    const char *pacman=alpm_find_tool("pacman");
    if(pacman==NULL){
        return 0;
    }

    char *argv[]={(char *)pacman,"-Qu","--color","never",NULL};
    char *text=NULL;
    int status=alpm_run(argv,&text);
    if(text==NULL){
        return 0;
    }

    char *cursor=text;
    while(cursor!=NULL&&*cursor!='\0'){
        char *line=cursor;
        char *newline=strchr(cursor,'\n');
        if(newline!=NULL){
            *newline='\0';
            cursor=newline+1;
        }else{
            cursor=NULL;
        }

        char *space=strchr(line,' ');
        if(space==NULL){
            continue;
        }
        *space='\0';
        char *name=line;
        char *arrow=strstr(space+1," -> ");
        if(arrow==NULL){
            continue;
        }
        char *new_version=arrow+4;
        while(*new_version==' '||*new_version=='\t'){
            new_version++;
        }
        char *end=new_version;
        while(*end!='\0'&&*end!=' '&&*end!='\t'){
            end++;
        }
        *end='\0';
        if(!alpm_name_valid(name)||new_version[0]=='\0'){
            continue;
        }

        package_t *pkg=pacman_installed_find(alpm_self(),name);
        if(pkg==NULL||pkg->foreign){
            continue;
        }
        char *copy=alpm_strdup(new_version);
        if(copy==NULL){
            continue;
        }
        free(pkg->new_version);
        pkg->new_version=copy;
        pkg->update_available=1;
    }
    free(text);
    return status==0?1:0;
}

/* 非 AUR 官方仓库的包操作。需要 root，优先使用 pkexec 触发 polkit；
 * 没有图形认证代理时退回到无交互 sudo -n。 */
static int alpm_run_package_action(package_t *pkg,int action){
    if(pkg==NULL||!alpm_name_valid(pkg->name)){
        return 0;
    }

    const char *pacman=alpm_find_tool("pacman");
    if(pacman==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"alpm: pacman binary not found");
        return 0;
    }

    const char *operation[4];
    size_t operation_count=0;
    if(action==0){
        /* 显式安装动作不使用 --needed：目标已经作为依赖安装过时，
         * pacman -S 会重装；--asexplicit 保证它进入 installed 页。
         * 否则依赖包重装后 reason 仍是 depend，无法出现在显式列表里。 */
        operation[operation_count++]="-S";
        operation[operation_count++]="--asexplicit";
        operation[operation_count++]="--noconfirm";
    }else if(action==1){
        /* 已安装包的 update/reinstall 也按显式处理：从搜索页对依赖包
         * 执行 reinstall 后，installed 页能看到它。 */
        operation[operation_count++]="-S";
        operation[operation_count++]="--asexplicit";
        operation[operation_count++]="--noconfirm";
    }else{
        operation[operation_count++]="-R";
        operation[operation_count++]="--noconfirm";
    }

    char *argv[12];
    size_t used=0;
    if(geteuid()==0){
        argv[used++]=(char *)pacman;
    }else{
        const char *pkexec=alpm_find_tool("pkexec");
        const char *sudo=alpm_find_tool("sudo");
        if(pkexec!=NULL){
            argv[used++]=(char *)pkexec;
            argv[used++]=(char *)pacman;
        }else if(sudo!=NULL){
            argv[used++]=(char *)sudo;
            argv[used++]="-n";
            argv[used++]=(char *)pacman;
        }else{
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "alpm: no pkexec/sudo available for package operation");
            return 0;
        }
    }
    for(size_t i=0;i<operation_count&&used+3<sizeof(argv)/sizeof(argv[0]);i++){
        argv[used++]=(char *)operation[i];
    }
    argv[used++]="--";
    argv[used++]=(char *)pkg->name;
    argv[used]=NULL;

    if(getenv("PACMAN_GUI_DRY_RUN")!=NULL){
        char command[512];
        size_t offset=0;
        command[0]='\0';
        for(size_t i=0;argv[i]!=NULL&&offset+1<sizeof(command);i++){
            int written=snprintf(command+offset,sizeof(command)-offset,
                "%s%s",i>0?" ":"",argv[i]);
            if(written<0||(size_t)written>=sizeof(command)-offset){
                break;
            }
            offset+=(size_t)written;
        }
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "alpm: dry-run command: %s",command);
        return 1;
    }

    /* 不要捕获 pacman 的输出：pkexec/hook 可能 fork 出继承 stdout 的
     * 子进程，父进程等 pipe EOF 会一直卡住，表现为包已经删掉但操作线程
     * 永远不结束，UI 也就不会刷新。动作只关心退出码。 */
    int status=alpm_run(argv,NULL);
    int ok=status==0;
    if(!ok){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "alpm: operation on %s failed (%d)",pkg->name,status);
    }
    return ok;
}

static int alpm_install_package(package_t *pkg){
    return alpm_run_package_action(pkg,0);
}

static int alpm_update_package(package_t *pkg){
    return alpm_run_package_action(pkg,1);
}

static int alpm_uninstall_package(package_t *pkg){
    return alpm_run_package_action(pkg,2);
}

static void alpm_foreign_clear(void){
    for(size_t i=0;i<alpm_foreign_name_count;i++){
        free(alpm_foreign_names[i]);
    }
    free(alpm_foreign_names);
    alpm_foreign_names=NULL;
    alpm_foreign_name_count=0;
    alpm_foreign_unknown=0;
}

static void alpm_foreign_add(const char *name){
    char **next=realloc(alpm_foreign_names,
        (alpm_foreign_name_count+1)*sizeof(*next));
    if(next==NULL){
        alpm_foreign_unknown=1;
        return;
    }
    alpm_foreign_names=next;
    char *copy=alpm_strdup(name);
    if(copy==NULL){
        alpm_foreign_unknown=1;
        return;
    }
    alpm_foreign_names[alpm_foreign_name_count++]=copy;
}

static int alpm_is_foreign_name(const char *name){
    if(alpm_foreign_unknown){
        return 1;
    }
    if(name==NULL){
        return 0;
    }
    for(size_t i=0;i<alpm_foreign_name_count;i++){
        if(strcmp(alpm_foreign_names[i],name)==0){
            return 1;
        }
    }
    return 0;
}

/* 供 packages 页识别作为依赖安装、未出现在显式 installed 列表里的包。 */
static int alpm_query_installed(package_t *pkg){
    if(pkg==NULL||pkg->name==NULL||!alpm_open()){
        return 0;
    }

    alpm_db_t *local=alpm_get_localdb(alpm_handle);
    if(local==NULL){
        return 0;
    }
    alpm_pkg_t *source=alpm_db_get_pkg(local,pkg->name);
    if(source==NULL){
        return 0;
    }

    char *version=alpm_strdup(alpm_pkg_get_version(source));
    if(version==NULL){
        return 0;
    }

    free(pkg->version);
    pkg->version=version;
    free(pkg->new_version);
    pkg->new_version=NULL;
    pkg->update_available=0;
    pkg->stat=PKG_STAT_INSTALLED;
    pkg->foreign=alpm_is_foreign_name(pkg->name)?1u:0u;
    return 1;
}

/* pacman -Qm 列出不在官方同步仓库里的已安装包（AUR/手动安装）。
 * 这些包不能假设 pacman -S 能找到同名官方包，因此 UI 不给它们操作按钮。
 * 如果 Qm 本身失败，保守地把所有 alpm 包标成 foreign，避免误操作。 */
static void alpm_mark_foreign(void){
    alpm_foreign_clear();
    size_t count=pacman_package_count();
    for(size_t i=0;i<count;i++){
        package_t *pkg=pacman_package_at(i);
        if(pkg!=NULL&&pkg->from==alpm_self()){
            pkg->foreign=0;
        }
    }

    const char *pacman=alpm_find_tool("pacman");
    if(pacman==NULL){
        alpm_foreign_unknown=1;
        for(size_t i=0;i<count;i++){
            package_t *pkg=pacman_package_at(i);
            if(pkg!=NULL&&pkg->from==alpm_self()){
                pkg->foreign=1;
            }
        }
        return;
    }

    char *argv[]={(char *)pacman,"-Qm","--color","never",NULL};
    char *text=NULL;
    int status=alpm_run(argv,&text);
    if(text==NULL||status!=0){
        free(text);
        alpm_foreign_unknown=1;
        for(size_t i=0;i<count;i++){
            package_t *pkg=pacman_package_at(i);
            if(pkg!=NULL&&pkg->from==alpm_self()){
                pkg->foreign=1;
            }
        }
        return;
    }

    char *cursor=text;
    while(cursor!=NULL&&*cursor!='\0'){
        char *line=cursor;
        char *newline=strchr(cursor,'\n');
        if(newline!=NULL){
            *newline='\0';
            cursor=newline+1;
        }else{
            cursor=NULL;
        }
        char *space=strchr(line,' ');
        if(space!=NULL){
            *space='\0';
        }
        if(!alpm_name_valid(line)){
            continue;
        }
        alpm_foreign_add(line);
        package_t *pkg=pacman_installed_find(alpm_self(),line);
        if(pkg!=NULL){
            pkg->foreign=1;
        }
    }
    free(text);
}

static void alpm_list_installed(void){
    if(alpm_listing_depth==0){
        if(!alpm_reopen()){
            return;
        }
    }else if(!alpm_open()){
        return;
    }
    alpm_listing_depth++;

    alpm_db_t *local=alpm_get_localdb(alpm_handle);
    if(local==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "alpm: local database unavailable: %s",
                     alpm_strerror(alpm_errno(alpm_handle)));
        alpm_listing_depth--;
        return;
    }

    for(alpm_list_t *i=alpm_db_get_pkgcache(local);i!=NULL;i=i->next){
        alpm_pkg_t *source=(alpm_pkg_t *)i->data;
        if(!alpm_append(source)){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "alpm: failed to add package %s",
                         alpm_pkg_get_name(source));
        }
    }
    alpm_mark_foreign();
    alpm_listing_depth--;
}

static int alpm_init(void){
    if(!pacman_alpm_available()){
        return 0;
    }
    return alpm_open();
}

static void alpm_quit(void){
    alpm_foreign_clear();
    if(alpm_handle!=NULL){
        alpm_release(alpm_handle);
        alpm_handle=NULL;
    }
}

static pacman_t *alpm_self(void){
    return &alpm_backend;
}

static pacman_t alpm_backend={
    .name="alpm",
    .official=1,
    .init=alpm_init,
    .check_updates=alpm_check_updates,
    .list_installed=alpm_list_installed,
    .search=alpm_search,
    .query_installed=alpm_query_installed,
    .install_package=alpm_install_package,
    .update_package=alpm_update_package,
    .uninstall_package=alpm_uninstall_package,
    .quit=alpm_quit
};

pacman_t *pacman_alpm(void){
    return &alpm_backend;
}
