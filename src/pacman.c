#include <stdlib.h>
#include <string.h>
#include "pacman.h"
#include "CDS/darray.h"
#ifdef HAVE_LIBALPM
#include "pacman_alpm.h"
#endif
#ifdef HAVE_HOMEBREW
#include "pacman_homebrew.h"
#endif

static pacman_t **backends=NULL;
static size_t backend_count=0;
static int defaults_registered=0;

/* 所有后端共用的已安装包视图：按 package_t.name 升序排列。
 * pkg_add/pkg_del 可能移动数组，pacman_package_at() 返回的指针只在
 * 下一次增删之前有效。 */
static darray_t(package_t) pkg_installed;
static int pkg_registry_ready=0;
static int pkg_resyncing=0;

/* ---- 已安装包注册表 ---- */

/* 释放由 pkg_add 接管的字符串字段。from/meta_data 是后端的常量字符串，
 * icon/data 也不归注册表所有，不能在这里释放。 */
static void pkg_free_fields(package_t *pkg){
    if(pkg==NULL){
        return;
    }
    free(pkg->name);
    free(pkg->version);
    free(pkg->description);
    if(pkg->licenses!=NULL){
        for(char **i=pkg->licenses;*i!=NULL;i++){
            free(*i);
        }
        free(pkg->licenses);
    }
    free(pkg->submitter);
}

static void pkg_registry_ensure(void){
    if(!pkg_registry_ready){
        da_init(&pkg_installed);
        pkg_registry_ready=1;
    }
}

static void pkg_registry_clear(void){
    for(size_t i=0;i<pkg_installed.size;i++){
        pkg_free_fields(&pkg_installed.arr[i]);
    }
    da_clear(&pkg_installed);
    pkg_registry_ready=0;
}

static int pkg_str_equal(const char *a,const char *b){
    if(a==NULL||b==NULL){
        return a==b;
    }
    return strcmp(a,b)==0;
}

static int pkg_same(const package_t *a,const package_t *b){
    return pkg_str_equal(a->from,b->from)
        &&pkg_str_equal(a->name,b->name)
        &&pkg_str_equal(a->meta_data,b->meta_data)
        &&pkg_str_equal(a->version,b->version);
}

/* 第一个 name 不小于 name 的下标 */
static size_t pkg_lower_bound(const char *name){
    size_t low=0;
    size_t high=pkg_installed.size;
    while(low<high){
        size_t mid=low+(high-low)/2;
        const char *mid_name=pkg_installed.arr[mid].name;
        if(mid_name==NULL||strcmp(mid_name,name)<0){
            low=mid+1;
        }else{
            high=mid;
        }
    }
    return low;
}

static int pkg_find(const package_t *key,size_t *index){
    if(key==NULL||key->name==NULL){
        return 0;
    }

    size_t i=pkg_lower_bound(key->name);
    for(;i<pkg_installed.size;i++){
        package_t *item=&pkg_installed.arr[i];
        if(!pkg_str_equal(item->name,key->name)){
            break;
        }
        if(pkg_same(item,key)){
            if(index!=NULL){
                *index=i;
            }
            return 1;
        }
    }
    return 0;
}

/* 注册表里没有这个包（或重复上报）时，以该管理器为准重建它的包列表。 */
static void pkg_resync(const char *from){
    if(from==NULL||pkg_resyncing){
        return;
    }
    pkg_resyncing=1;
    pkg_del_from((char *)from);

    pacman_t *backend=pacman_backend_find((char *)from);
    if(backend!=NULL&&backend->list_installed!=NULL){
        backend->list_installed();
    }
    pkg_resyncing=0;
}

void pkg_add(package_t *pkg){
    if(pkg==NULL){
        return;
    }
    pkg_registry_ensure();
    if(pkg->name==NULL){
        pkg_free_fields(pkg);
        return;
    }

    size_t index=pkg_lower_bound(pkg->name);
    for(size_t i=index;i<pkg_installed.size;i++){
        package_t *item=&pkg_installed.arr[i];
        if(!pkg_str_equal(item->name,pkg->name)){
            break;
        }
        if(pkg_same(item,pkg)){
            /* 相同包已经存在：说明该管理器的缓存过期，清掉后重新枚举。
             * 入参由 pkg_add 接管，去重时负责释放。 */
            const char *from=pkg->from;
            pkg_free_fields(pkg);
            pkg_resync(from);
            return;
        }
    }

    if(da_add_loc(sizeof(*pkg),&pkg_installed,pkg,index)!=0){
        pkg_free_fields(pkg);
    }
}

void pkg_del(package_t *pkg){
    if(pkg==NULL){
        return;
    }
    pkg_registry_ensure();

    size_t index=0;
    if(pkg_find(pkg,&index)){
        pkg_free_fields(&pkg_installed.arr[index]);
        da_del(sizeof(package_t),&pkg_installed,index);
        return;
    }

    /* 没找到说明注册表和后端不一致，按这个包的管理器重新同步 */
    pkg_resync(pkg->from);
}

void pkg_del_from(char *name){
    if(name==NULL){
        return;
    }
    pkg_registry_ensure();

    for(size_t i=pkg_installed.size;i>0;i--){
        package_t *pkg=&pkg_installed.arr[i-1];
        if(pkg->from!=NULL&&strcmp(pkg->from,name)==0){
            pkg_free_fields(pkg);
            da_del(sizeof(package_t),&pkg_installed,i-1);
        }
    }
}

size_t pacman_package_count(void){
    pkg_registry_ensure();
    return pkg_installed.size;
}

package_t *pacman_package_at(size_t index){
    pkg_registry_ensure();
    if(index>=pkg_installed.size){
        return NULL;
    }
    return &pkg_installed.arr[index];
}

/* ---- 后端注册表 ---- */

static void register_default_backends(void){
    if(defaults_registered){
        return;
    }
    defaults_registered=1;
#ifdef HAVE_LIBALPM
    if(pacman_alpm_available()){
        pacman_register(pacman_alpm());
    }
#endif
#ifdef HAVE_HOMEBREW
    if(pacman_homebrew_available()){
        pacman_register(pacman_homebrew());
    }
#endif
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
    pkg_registry_clear();

    int initialized=0;
    for(size_t i=0;i<backend_count;i++){
        pacman_t *backend=backends[i];
        if(backend==NULL){
            continue;
        }
        if(backend->init==NULL||backend->init()){
            initialized++;
        }
    }
    return initialized;
}

/* 枚举所有已注册后端的已安装包。这个函数会写入 pkg_installed，
 * 调用期间 UI 不应并发读取包列表；常见用法是在后台线程列出，
 * 完成后由主线程重建卡片。 */
void pacman_list_installed_all(void){
    register_default_backends();
    for(size_t i=0;i<backend_count;i++){
        pacman_t *backend=backends[i];
        if(backend!=NULL&&backend->list_installed!=NULL){
            backend->list_installed();
        }
    }
}

void pacman_quit_all(void){
    for(size_t i=0;i<backend_count;i++){
        pacman_t *backend=backends[i];
        if(backend!=NULL&&backend->quit!=NULL){
            backend->quit();
        }
    }
    pkg_registry_clear();
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

pacman_t *pacman_backend_find(char *name){
    if(name==NULL){
        return NULL;
    }
    register_default_backends();

    for(size_t i=0;i<backend_count;i++){
        pacman_t *backend=backends[i];
        if(backend!=NULL&&backend->name!=NULL&&strcmp(backend->name,name)==0){
            return backend;
        }
    }
    return NULL;
}
