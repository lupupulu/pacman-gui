#include <stdlib.h>
#include "pacman.h"
#include "pacman_homebrew.h"

static pacman_t **backends=NULL;
static size_t backend_count=0;
static int defaults_registered=0;

static void register_default_backends(void){
    if(defaults_registered){
        return;
    }
    defaults_registered=1;
    pacman_register(pacman_homebrew());
}

void pacman_register(pacman_t *m){
    if(m==NULL){
        return;
    }

    for(size_t i=0;i<backend_count;i++){
        if(backends[i]==m){
            return;
        }
    }

    pacman_t **next=realloc(backends,(backend_count+1)*sizeof(*next));
    if(next==NULL){
        return;
    }
    backends=next;
    backends[backend_count++]=m;
}

int pacman_init_all(void){
    register_default_backends();

    int initialized=0;
    for(size_t i=0;i<backend_count;i++){
        pacman_t *backend=backends[i];
        if(backend->init==NULL||backend->init()){
            initialized++;
        }
    }
    return initialized;
}

void pacman_quit_all(void){
    for(size_t i=0;i<backend_count;i++){
        pacman_t *backend=backends[i];
        if(backend!=NULL&&backend->quit!=NULL){
            backend->quit();
        }
    }
}

size_t pacman_backend_count(void){
    register_default_backends();
    return backend_count;
}

pacman_t *pacman_backend_at(size_t index){
    register_default_backends();
    if(index>=backend_count){
        return NULL;
    }
    return backends[index];
}

size_t pacman_package_count(void){
    register_default_backends();

    size_t total=0;
    for(size_t i=0;i<backend_count;i++){
        pacman_t *backend=backends[i];
        if(backend!=NULL){
            total+=backend->pkg_cnt;
        }
    }
    return total;
}

package_t *pacman_package_at(size_t index){
    register_default_backends();

    for(size_t i=0;i<backend_count;i++){
        pacman_t *backend=backends[i];
        if(backend==NULL){
            continue;
        }
        if(index<backend->pkg_cnt){
            return &backend->pkgs[index];
        }
        index-=backend->pkg_cnt;
    }
    return NULL;
}
