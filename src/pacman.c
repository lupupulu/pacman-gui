#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pacman.h"
#include "sdlgui_widget.h"
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
/* darray_t(package_t) 每次展开都是不同的匿名结构体，用来做函数参数会出现
 * 指针类型不兼容的警告；这里显式命名一次，da_* 仍然按 void* 操作。 */
typedef struct {
    package_t *arr;
    size_t size;
    size_t real;
}package_array_t;

static package_array_t pkg_installed;
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

/* 第一个 name 不小于 name 的下标；注册表和可用包注册表共用 */
static size_t pkg_array_lower_bound(const package_array_t *array,const char *name){
    size_t low=0;
    size_t high=array->size;
    while(low<high){
        size_t mid=low+(high-low)/2;
        const char *mid_name=array->arr[mid].name;
        if(mid_name==NULL||strcmp(mid_name,name)<0){
            low=mid+1;
        }else{
            high=mid;
        }
    }
    return low;
}

static size_t pkg_lower_bound(const char *name){
    return pkg_array_lower_bound(&pkg_installed,name);
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

/* 按 from+name 在已安装注册表里查找；找不到返回 NULL。
 * 供 packages 页把搜索结果标记成“已安装”。 */
package_t *pacman_installed_find(const char *from,const char *name){
    if(name==NULL){
        return NULL;
    }
    pkg_registry_ensure();

    size_t i=pkg_lower_bound(name);
    for(;i<pkg_installed.size;i++){
        package_t *item=&pkg_installed.arr[i];
        if(!pkg_str_equal(item->name,name)){
            break;
        }
        if(pkg_str_equal(item->from,from)){
            return item;
        }
    }
    return NULL;
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
    pacman_available_clear();
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

/* ---- 可安装包（packages 栏目）注册表 ----
 * 与已安装注册表分开：这里存后端搜索出来的“可安装”包，stat 通常为
 * PKG_STAT_NULL。搜索在后台线程写、主线程 join 后才读，约定与
 * pacman_list_installed_all() 相同。 */
static package_array_t pkg_available;
static int pkg_available_ready=0;

static void pkg_available_ensure(void){
    if(!pkg_available_ready){
        da_init(&pkg_available);
        pkg_available_ready=1;
    }
}

void pacman_available_clear(void){
    pkg_available_ensure();
    for(size_t i=0;i<pkg_available.size;i++){
        pkg_free_fields(&pkg_available.arr[i]);
    }
    da_clear(&pkg_available);
    pkg_available_ready=0;
}

/* 接管 pkg 的字符串字段；按 from+name+meta_data 去重（不触发 resync，
 * 避免在搜索线程里回头写已安装注册表）。重复时负责释放入参。
 * 结果保持后端的返回顺序，不按字母排序：相关性由后端决定，直接追加即可。 */
void pkg_available_add(package_t *pkg){
    if(pkg==NULL){
        return;
    }
    pkg_available_ensure();
    if(pkg->name==NULL){
        pkg_free_fields(pkg);
        return;
    }

    /* 没有排序就不能二分，结果量级只有几十到几百条，线性去重足够 */
    for(size_t i=0;i<pkg_available.size;i++){
        package_t *item=&pkg_available.arr[i];
        if(pkg_str_equal(item->name,pkg->name)&&
           pkg_str_equal(item->from,pkg->from)&&
           pkg_str_equal(item->meta_data,pkg->meta_data)){
            pkg_free_fields(pkg);
            return;
        }
    }

    if(da_add(sizeof(*pkg),&pkg_available,pkg)!=0){
        pkg_free_fields(pkg);
    }
}

size_t pacman_available_count(void){
    pkg_available_ensure();
    return pkg_available.size;
}

package_t *pacman_available_at(size_t index){
    pkg_available_ensure();
    if(index>=pkg_available.size){
        return NULL;
    }
    return &pkg_available.arr[index];
}

/* 清空可安装注册表并让所有支持搜索的后端按 keyword 填充。
 * 只应在后台线程调用；主线程 join 之后再读 pacman_available_*。 */
void pacman_search_all(const char *keyword){
    register_default_backends();
    pacman_available_clear();
    if(keyword==NULL||keyword[0]=='\0'){
        return;
    }
    for(size_t i=0;i<backend_count;i++){
        pacman_t *backend=backends[i];
        if(backend!=NULL&&backend->search!=NULL){
            backend->search(keyword);
        }
    }
}

/* ---- 软件包卡片（原 pacman_gui.c，迁移至此） ---- */

#define PACKAGE_CARD_HEIGHT 104.0f
#define PACKAGE_MARGIN 16.0f
#define PACKAGE_BUTTON_GAP 8.0f
#define PACKAGE_PRIMARY_WIDTH 148.0f
#define PACKAGE_DELETE_WIDTH PACKAGE_PRIMARY_WIDTH

typedef struct {
    layout_rect_t rect;
    package_t *package;

    layout_label_t *name;
    layout_label_t *version;
    layout_label_t *people;
    layout_label_t *license;

    widget_button_t *primary;
    widget_button_t *remove;
}layout_package_t;

static void package_set_box(layout_t *l,float x,float y,float w,float h){
    if(l==NULL){
        return;
    }
    l->x=x;
    l->y=y;
    l->width=w;
    l->height=h;
}

static void package_draw(void *p){
    layout_package_t *card=p;
    if(card==NULL){
        return;
    }

    rect_draw(&card->rect);

    SDL_Renderer *renderer=get_renderer();
    if(renderer==NULL){
        return;
    }

    unsigned color=get_divider_color();
    SDL_SetRenderDrawColor(renderer,
        (Uint8)((color>>24)&0xFF),
        (Uint8)((color>>16)&0xFF),
        (Uint8)((color>>8)&0xFF),
        (Uint8)(color&0xFF));
    float y=card->rect.l.y+card->rect.l.height-1.0f;
    SDL_RenderLine(renderer,
        card->rect.l.x+PACKAGE_MARGIN,y,
        card->rect.l.x+card->rect.l.width-PACKAGE_MARGIN,y);
}

static void package_update(void *p){
    layout_package_t *card=p;
    if(card==NULL){
        return;
    }

    layout_update(&card->rect.l);
    float width=card->rect.l.width;
    float height=card->rect.l.height;
    if(width<=0.0f||height<=0.0f){
        return;
    }

    package_t *package=card->package;
    int installed=package!=NULL&&package->stat!=PKG_STAT_NULL;
    float info_width=width-PACKAGE_MARGIN*2.0f-PACKAGE_PRIMARY_WIDTH-PACKAGE_MARGIN;
    if(info_width<40.0f){
        info_width=40.0f;
    }

    float left=card->rect.l.x+PACKAGE_MARGIN;
    float top=card->rect.l.y;
    package_set_box(&card->name->l,left,top+8.0f,info_width,22.0f);
    package_set_box(&card->version->l,left,top+32.0f,info_width,20.0f);
    if(card->people!=NULL){
        package_set_box(&card->people->l,left,top+54.0f,info_width,20.0f);
        if(card->license!=NULL){
            package_set_box(&card->license->l,left,top+76.0f,info_width,20.0f);
        }
    }else if(card->license!=NULL){
        package_set_box(&card->license->l,left,top+60.0f,info_width,20.0f);
    }

    float button_height=get_theme()->button_height;
    float primary_x=card->rect.l.x+width-PACKAGE_MARGIN-PACKAGE_PRIMARY_WIDTH;
    if(card->primary!=NULL){
        float button_y=card->rect.l.y+(height-button_height)/2.0f;
        if(installed){
            float total=button_height*2.0f+PACKAGE_BUTTON_GAP;
            button_y=card->rect.l.y+(height-total)/2.0f;
        }
        package_set_box(&card->primary->l,primary_x,button_y,
            PACKAGE_PRIMARY_WIDTH,button_height);
    }

    if(card->remove!=NULL){
        float remove_x=card->rect.l.x+width-PACKAGE_MARGIN-PACKAGE_DELETE_WIDTH;
        float remove_y=card->rect.l.y+(height-button_height)/2.0f;
        if(installed){
            float total=button_height*2.0f+PACKAGE_BUTTON_GAP;
            remove_y=card->rect.l.y+(height-total)/2.0f+button_height+PACKAGE_BUTTON_GAP;
        }
        package_set_box(&card->remove->l,remove_x,remove_y,
            PACKAGE_DELETE_WIDTH,button_height);
    }
}

static layout_label_t *package_label(layout_t *card,TTF_Font *font,
    const char *text,unsigned color){
    layout_label_t *label=create_layout_label(
        card,
        120.0f,
        18.0f,
        make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f),
        make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f),
        font,
        text,
        color
    );
    if(label!=NULL){
        label->l.align=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    }
    return label;
}

layout_rect_t *create_layout_package(package_t *package){
    if(package==NULL){
        return NULL;
    }

    layout_package_t *card=create_layout(
        sizeof(*card),
        NULL,
        0.0f,
        PACKAGE_CARD_HEIGHT,
        package_draw,
        package_update,
        NULL,
        NULL,
        make_align(0,0,1,0,0.0f,0.0f,1.0f,0.0f)
    );
    if(card==NULL){
        return NULL;
    }

    card->package=package;
    card->rect.color=get_color(COLOR_MAIN);

    TTF_Font *subtitle=obtain_font(get_font(TEXT_SUBTITLE),get_text_size(TEXT_SUBTITLE));
    TTF_Font *context=obtain_font(get_font(TEXT_CONTEXT),get_text_size(TEXT_CONTEXT));
    if(subtitle==NULL||context==NULL){
        delete_layout((layout_t *)card);
        return NULL;
    }

    const char *name=package->name!=NULL?package->name:"-";
    const char *version=package->version!=NULL?package->version:"-";
    const char *submitter=package->submitter;
    char **licenses=package->licenses;
    char maintainer[256];
    maintainer[0]='\0';
    size_t maintainer_used=0;
    if(package->maintainers!=NULL){
        for(size_t i=0;package->maintainers[i]!=NULL;i++){
            const char *item=package->maintainers[i];
            if(item==NULL||item[0]=='\0'){
                continue;
            }
            int written=snprintf(maintainer+maintainer_used,
                sizeof(maintainer)-maintainer_used,
                "%s%s",maintainer_used>0?", ":"",item);
            if(written<0||(size_t)written>=sizeof(maintainer)-maintainer_used){
                break;
            }
            maintainer_used+=(size_t)written;
        }
    }
    card->name=package_label((layout_t *)card,subtitle,name,get_text_color(TEXT_TITLE));

    char version_text[256];
    snprintf(version_text,sizeof(version_text),"Version: %s",version);
    card->version=package_label((layout_t *)card,context,version_text,get_text_color(TEXT_CONTEXT));

    int has_maintainer=maintainer[0]!='\0';
    int has_submitter=submitter!=NULL&&submitter[0]!='\0';
    if(has_maintainer||has_submitter){
        char people_text[512];
        people_text[0]='\0';
        if(has_maintainer){
            snprintf(people_text,sizeof(people_text),"Maintainer: %s",maintainer);
        }
        if(has_submitter){
            size_t used=strlen(people_text);
            snprintf(people_text+used,sizeof(people_text)-used,
                     "%sSubmitter: %s",used>0?"    ":"",submitter);
        }
        card->people=package_label((layout_t *)card,context,people_text,
            get_text_color(TEXT_CONTEXT));
        if(card->people==NULL){
            delete_layout((layout_t *)card);
            return NULL;
        }
    }

    int has_license=0;
    if(licenses!=NULL){
        for(size_t i=0;licenses[i]!=NULL;i++){
            if(licenses[i][0]!='\0'){
                has_license=1;
                break;
            }
        }
    }
    if(has_license){
        char license_text[512];
        size_t used=(size_t)snprintf(license_text,sizeof(license_text),"License: ");
        if(used>=sizeof(license_text)){
            used=sizeof(license_text)-1;
        }
        int first=1;
        for(size_t i=0;licenses[i]!=NULL;i++){
            const char *item=licenses[i];
            if(item==NULL||item[0]=='\0'){
                continue;
            }
            int written=snprintf(license_text+used,sizeof(license_text)-used,
                "%s%s",first?"":", ",item);
            if(written<0||(size_t)written>=sizeof(license_text)-used){
                break;
            }
            used+=(size_t)written;
            first=0;
        }
        card->license=package_label((layout_t *)card,context,license_text,
            get_text_color(TEXT_CONTEXT));
    }

    if(card->name==NULL||card->version==NULL||
       (has_license&&card->license==NULL)){
        delete_layout((layout_t *)card);
        return NULL;
    }

    const char *primary_text=package->stat==PKG_STAT_NULL
        ?"\\install install"
        :"\\update update";
    card->primary=widget_create_button(
        card,
        PACKAGE_PRIMARY_WIDTH,
        make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f),
        primary_text,
        NULL,
        NULL
    );
    if(card->primary!=NULL){
        card->primary->l.align=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    }

    if(package->stat!=PKG_STAT_NULL){
        card->remove=widget_create_button(
            card,
            PACKAGE_DELETE_WIDTH,
            make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f),
            "\\delete delete",
            NULL,
            NULL
        );
        if(card->remove!=NULL){
            card->remove->l.align=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
        }
    }

    package_update(card);
    return &card->rect;
}
