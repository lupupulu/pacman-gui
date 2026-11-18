#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pacman_alpm.h"
#include <alpm.h>

static alpm_handle_t *alpm_handle=NULL;

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

/* 读取一个 alpm 包并交给全局注册表，字段所有权同时转交 pkg_add()。
 * 只列出用户显式安装的包；作为依赖自动安装的包不进入 UI。 */
static int alpm_append(alpm_pkg_t *source){
    if(alpm_pkg_get_reason(source)!=ALPM_PKG_REASON_EXPLICIT){
        return 1;
    }

    package_t pkg;
    memset(&pkg,0,sizeof(pkg));

    pkg.from="alpm";
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

static void alpm_list_installed(void){
    if(!alpm_open()){
        return;
    }

    alpm_db_t *local=alpm_get_localdb(alpm_handle);
    if(local==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "alpm: local database unavailable: %s",
                     alpm_strerror(alpm_errno(alpm_handle)));
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
}

static int alpm_init(void){
    if(!pacman_alpm_available()){
        return 0;
    }
    return alpm_open();
}

static void alpm_quit(void){
    if(alpm_handle!=NULL){
        alpm_release(alpm_handle);
        alpm_handle=NULL;
    }
}

static pacman_t alpm_backend={
    .name="alpm",
    .init=alpm_init,
    .list_installed=alpm_list_installed,
    .quit=alpm_quit
};

pacman_t *pacman_alpm(void){
    return &alpm_backend;
}
