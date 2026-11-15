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

static char *alpm_join_licenses(alpm_list_t *licenses){
    if(licenses==NULL){
        return NULL;
    }

    size_t length=1;
    for(alpm_list_t *i=licenses;i!=NULL;i=i->next){
        if(i->data!=NULL){
            length+=strlen((const char *)i->data)+2;
        }
    }

    char *text=malloc(length);
    if(text==NULL){
        return NULL;
    }
    text[0]='\0';
    size_t offset=0;
    for(alpm_list_t *i=licenses;i!=NULL;i=i->next){
        if(i->data==NULL){
            continue;
        }
        if(offset>0){
            text[offset++]=',';
            text[offset++]=' ';
        }
        const char *license=(const char *)i->data;
        size_t license_length=strlen(license);
        memcpy(text+offset,license,license_length);
        offset+=license_length;
    }
    text[offset]='\0';
    return text;
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

/* 读取一个 alpm 包并交给全局注册表，字段所有权同时转交 pkg_add()。 */
static int alpm_append(alpm_pkg_t *source){
    package_t pkg;
    memset(&pkg,0,sizeof(pkg));

    pkg.from="alpm";
    pkg.meta_data="local";
    pkg.name=alpm_strdup(alpm_pkg_get_name(source));
    pkg.version=alpm_strdup(alpm_pkg_get_version(source));
    pkg.description=alpm_strdup(alpm_pkg_get_desc(source));
    pkg.license=alpm_join_licenses(alpm_pkg_get_licenses(source));
    pkg.submitter=alpm_strdup(alpm_pkg_get_packager(source));
    pkg.stat=PKG_STAT_INSTALLED;
    pkg.progress=0.0;

    off_t installed_size=alpm_pkg_get_isize(source);
    pkg.size=installed_size>=0?(unsigned long)installed_size:(unsigned long)-1;

    if(pkg.name==NULL||pkg.version==NULL){
        free(pkg.name);
        free(pkg.version);
        free(pkg.description);
        free(pkg.license);
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
