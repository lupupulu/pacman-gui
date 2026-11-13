#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include "pacman_homebrew.h"

static char *homebrew_strdup(const char *text){
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

static void homebrew_free(void){
    pacman_t *backend=pacman_homebrew();
    for(size_t i=0;i<backend->pkg_cnt;i++){
        free(backend->pkgs[i].name);
        free(backend->pkgs[i].version);
    }
    free(backend->pkgs);
    backend->pkgs=NULL;
    backend->pkg_cnt=0;
}

static int homebrew_append(const char *name,const char *version,const char *meta){
    pacman_t *backend=pacman_homebrew();
    package_t *pkgs=realloc(backend->pkgs,(backend->pkg_cnt+1)*sizeof(*pkgs));
    if(pkgs==NULL){
        return 0;
    }
    backend->pkgs=pkgs;

    package_t *pkg=&backend->pkgs[backend->pkg_cnt];
    memset(pkg,0,sizeof(*pkg));
    pkg->from="homebrew";
    pkg->meta_data=(char *)meta;
    pkg->name=homebrew_strdup(name);
    pkg->version=homebrew_strdup(version);
    pkg->stat=PKG_STAT_INSTALLED;
    pkg->size=(unsigned long)-1;

    if(pkg->name==NULL||pkg->version==NULL){
        free(pkg->name);
        free(pkg->version);
        memset(pkg,0,sizeof(*pkg));
        return 0;
    }

    backend->pkg_cnt++;
    return 1;
}

static const char *homebrew_binary(void){
    const char *prefix=getenv("HOMEBREW_PREFIX");
    static char path[PATH_MAX];

    if(prefix!=NULL&&prefix[0]!='\0'){
        snprintf(path,sizeof(path),"%s/bin/brew",prefix);
        if(access(path,X_OK)==0){
            return path;
        }
    }

    static const char *candidates[]={
        "/opt/homebrew/bin/brew",
        "/usr/local/bin/brew",
        "/home/linuxbrew/.linuxbrew/bin/brew"
    };
    for(size_t i=0;i<sizeof(candidates)/sizeof(candidates[0]);i++){
        if(access(candidates[i],X_OK)==0){
            return candidates[i];
        }
    }

    return "brew";
}

/* 读取 `brew list --versions --formula/--cask` 的文本输出。
 * 每行格式：name version [version ...]，取最后一个版本。 */
static int homebrew_list(const char *flag,const char *meta){
    char command[PATH_MAX+64];
    snprintf(command,sizeof(command),"%s list --versions %s",homebrew_binary(),flag);

    FILE *pipe=popen(command,"r");
    if(pipe==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "homebrew: failed to run %s",command);
        return 0;
    }

    char line[1024];
    int ok=1;
    while(fgets(line,sizeof(line),pipe)!=NULL){
        size_t length=strlen(line);
        while(length>0&&(line[length-1]=='\n'||line[length-1]=='\r')){
            line[--length]='\0';
        }
        if(line[0]=='\0'){
            continue;
        }

        char *name=line;
        while(*name==' '||*name=='\t'){
            name++;
        }
        char *cursor=name;
        while(*cursor!='\0'&&*cursor!=' '&&*cursor!='\t'){
            cursor++;
        }
        if(*cursor!='\0'){
            *cursor++='\0';
        }
        while(*cursor==' '||*cursor=='\t'){
            cursor++;
        }

        char *end=cursor+strlen(cursor);
        while(end>cursor&&(end[-1]==' '||end[-1]=='\t')){
            *--end='\0';
        }

        const char *version=cursor;
        char *last=cursor;
        for(char *p=cursor;*p!='\0';){
            while(*p==' '||*p=='\t'){
                p++;
            }
            if(*p=='\0'){
                break;
            }
            last=p;
            while(*p!='\0'&&*p!=' '&&*p!='\t'){
                p++;
            }
        }
        version=last;

        if(name[0]!='\0'&&!homebrew_append(name,version,meta)){
            ok=0;
            break;
        }
    }

    int status=pclose(pipe);
    if(status!=0){
        ok=0;
    }
    return ok;
}

static int homebrew_init(void){
    homebrew_free();
    int formula=homebrew_list("--formula","formula");
    int cask=homebrew_list("--cask","cask");
    return formula||cask;
}

static int homebrew_refresh(void){
    return homebrew_init();
}

static void homebrew_quit(void){
    homebrew_free();
}

static pacman_t homebrew_backend={
    .name="homebrew",
    .init=homebrew_init,
    .refresh=homebrew_refresh,
    .quit=homebrew_quit
};

pacman_t *pacman_homebrew(void){
    return &homebrew_backend;
}
