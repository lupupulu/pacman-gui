#include <SDL3/SDL.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdlgui.h"
#include "sdlgui_widget.h"
#include "sdlgui_i18n.h"
#include "pacman.h"
#include "init.h"

#define PAGE_MARGIN 16.0f
#define PAGE_SEARCH_GAP 8.0f
/* 每帧最多创建的卡片数：搜索输入、后台枚举时都保持界面可响应 */
#define PAGE_CARDS_PER_FRAME 12
/* 停止输入多久后才发起后端搜索，避免每敲一个字符都起一个 brew 进程 */
#define PACKAGES_SEARCH_DELAY 350

/* 通过 pacman.c 的统一包视图生成列表。 */
static layout_list_t *create_package_list(layout_t *parent){
    layout_list_t *list=create_layout_list(
        parent,
        0.0f,
        0.0f,
        make_align(3,3,1,1,0.0f,0.0f,1.0f,1.0f),
        0,
        NULL
    );
    if(list==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to create package list"));
        return NULL;
    }
    return list;
}

static char *page_strdup(const char *text){
    if(text==NULL){
        return NULL;
    }
    size_t size=strlen(text)+1;
    char *copy=malloc(size);
    if(copy!=NULL){
        memcpy(copy,text,size);
    }
    return copy;
}

/* ---- packages 页：搜索可安装包 ---- */

typedef struct {
    layout_rect_t rect;
    layout_list_t *list;
    widget_slider_t *slider;
    widget_input_t *search;
    layout_label_t *intro_hint;     /* 还没输入关键词 */
    layout_label_t *searching_hint; /* 正在等后端结果 */
    layout_label_t *empty_hint;     /* 搜索完成但没有结果 */

    char keyword[128];          /* 搜索框当前内容 */
    char running_keyword[128];  /* 后台线程正在搜的关键词 */
    size_t scan_index;          /* 已经生成卡片的可用包下标 */
    size_t scan_total;          /* 本次结果总数，join 后固定 */
    float header_offset;        /* selector 占用的高度，搜索框从这里开始 */
    unsigned long change_time;  /* 最近一次输入时间，用于防抖 */

    unsigned keyword_dirty:1;
    unsigned scan_running:1;
    unsigned searching:1;       /* 后台搜索线程在跑 */

    SDL_AtomicInt results_ready;
    pthread_t thread;
}packages_page_t;

static packages_page_t *packages_page_view=NULL;

static void packages_page_sync_hints(packages_page_t *page);

/* 后台线程：只写 pacman 的可安装注册表，主线程 join 后才读 */
static void *packages_page_search_thread(void *data){
    packages_page_t *page=data;
    pacman_search_all(page->running_keyword);
    SDL_SetAtomicInt(&page->results_ready,1);
    return NULL;
}

/* 搜索结果里已经装过的包标成已安装并补上版本号，卡片会显示 update/delete */
static void packages_page_mark_installed(void){
    if(!pacman_installed_ready()){
        return;
    }
    size_t count=pacman_available_count();
    for(size_t i=0;i<count;i++){
        package_t *package=pacman_available_at(i);
        if(package==NULL||package->name==NULL){
            continue;
        }
        package_t *installed=pacman_installed_find(package->from,package->name);
        if(installed==NULL){
            /* 依赖包不在 installed 页的显式列表里，但仍然算已安装。 */
            if(pacman_query_installed(package)){
                continue;
            }
            /* 之前标记成已安装、但已经被删掉的搜索结果恢复为可安装。 */
            package->stat=PKG_STAT_NULL;
            package->foreign=0;
            package->update_available=0;
            free(package->new_version);
            package->new_version=NULL;
            continue;
        }
        package->stat=PKG_STAT_INSTALLED;
        package->foreign=installed->foreign;
        char *version=page_strdup(installed->version);
        if(version!=NULL){
            free(package->version);
            package->version=version;
        }
        free(package->new_version);
        package->new_version=NULL;
        package->update_available=0;
        if(installed->update_available&&installed->new_version!=NULL){
            char *new_version=page_strdup(installed->new_version);
            if(new_version!=NULL){
                package->new_version=new_version;
                package->update_available=1;
            }
        }
    }
}

static void packages_page_finish_search(packages_page_t *page){
    if(page->searching){
        pthread_join(page->thread,NULL);
        page->searching=0;
    }

    packages_page_mark_installed();
    page->scan_index=0;
    page->scan_total=pacman_available_count();
    page->scan_running=page->scan_total>0?1:0;
    packages_page_sync_hints(page);
}

/* 清空当前结果并发起一次搜索；关键词为空时只清空，不启动线程 */
static void packages_page_start_search(packages_page_t *page){
    if(page==NULL||page->searching){
        return;
    }

    if(page->list!=NULL){
        list_clear(page->list);
        list_set_slide(page->list,0.0f);
    }
    page->scan_index=0;
    page->scan_total=0;
    page->scan_running=0;
    if(page->slider!=NULL){
        page->slider->all_height=0.0f;
        widget_slider_set(page->slider,0.0f);
    }

    if(page->keyword[0]=='\0'){
        pacman_available_clear();
        packages_page_sync_hints(page);
        return;
    }

    snprintf(page->running_keyword,sizeof(page->running_keyword),"%s",page->keyword);
    SDL_SetAtomicInt(&page->results_ready,0);
    if(pthread_create(&page->thread,NULL,packages_page_search_thread,page)!=0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("Failed to create the package search thread"));
        /* 线程起不来时退回同步搜索，保证功能仍可用 */
        pacman_search_all(page->running_keyword);
        packages_page_finish_search(page);
        return;
    }
    page->searching=1;
    packages_page_sync_hints(page);
}

static void packages_page_sync_hints(packages_page_t *page){
    if(page==NULL){
        return;
    }

    int empty=page->list==NULL||page->list->sons.size==0;
    int has_keyword=page->keyword[0]!='\0';
    int busy=page->searching||page->scan_running||page->keyword_dirty;
    int show_intro=!has_keyword&&empty&&!busy;
    int show_searching=has_keyword&&busy&&empty;
    int show_empty=has_keyword&&empty&&!busy;

    if(page->intro_hint!=NULL){
        page->intro_hint->l.enable=show_intro?1:0;
    }
    if(page->searching_hint!=NULL){
        page->searching_hint->l.enable=show_searching?1:0;
    }
    if(page->empty_hint!=NULL){
        page->empty_hint->l.enable=show_empty?1:0;
    }
}

static void packages_page_scan(packages_page_t *page){
    if(page==NULL||page->list==NULL||!page->scan_running){
        return;
    }

    size_t budget=PAGE_CARDS_PER_FRAME;
    while(page->scan_index<page->scan_total&&budget>0){
        size_t index=page->scan_index++;
        package_t *package=pacman_available_at(index);
        if(package==NULL){
            continue;
        }
        budget--;

        layout_rect_t *card=create_layout_package(package);
        if(card==NULL||!list_add(page->list,(layout_t *)card)){
            if(card!=NULL){
                delete_layout((layout_t *)card);
            }
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("Failed to create package card %zu"),index);
        }
    }

    if(page->scan_index>=page->scan_total){
        page->scan_running=0;
    }
}

static void packages_page_draw(void *p){
    packages_page_t *page=p;
    if(page!=NULL){
        rect_draw(&page->rect);
    }
}

static void packages_page_update(void *p){
    packages_page_t *page=p;
    if(page==NULL){
        return;
    }

    layout_update(&page->rect.l);

    unsigned long now=SDL_GetTicks();
    if(page->keyword_dirty&&!page->searching&&
       now-page->change_time>=PACKAGES_SEARCH_DELAY){
        page->keyword_dirty=0;
        packages_page_start_search(page);
    }
    if(page->searching&&SDL_GetAtomicInt(&page->results_ready)){
        packages_page_finish_search(page);
    }

    packages_page_scan(page);
    packages_page_sync_hints(page);

    float width=page->rect.l.width;
    float hint_y=0.0f;
    if(width>0.0f){
        if(page->search!=NULL){
            page->search->l.x=page->rect.l.x+PAGE_MARGIN;
            page->search->l.y=page->rect.l.y+page->header_offset;
            page->search->l.width=width-PAGE_MARGIN*2.0f;
            hint_y=page->search->l.y+page->search->l.height+32.0f;
        }

        layout_label_t *hints[]={
            page->intro_hint,
            page->searching_hint,
            page->empty_hint
        };
        for(size_t i=0;i<sizeof(hints)/sizeof(hints[0]);i++){
            if(hints[i]==NULL){
                continue;
            }
            hints[i]->l.x=page->rect.l.x+PAGE_MARGIN;
            hints[i]->l.y=hint_y;
            hints[i]->l.width=width-PAGE_MARGIN*2.0f;
            hints[i]->l.height=32.0f;
        }
    }

    if(page->list!=NULL&&page->slider!=NULL){
        page->slider->all_height=list_get_content_height(page->list);
        widget_slider_set(page->slider,list_get_slide(page->list));
    }
}

/* 搜索框变化：只记录关键词和输入时间，真正的搜索在 update 里防抖触发 */
static void packages_page_search_change(const char *text,void *data){
    packages_page_t *page=data;
    if(page==NULL){
        return;
    }

    if(text==NULL){
        text="";
    }
    if(strcmp(page->keyword,text)==0){
        return;
    }

    snprintf(page->keyword,sizeof(page->keyword),"%s",text);
    page->change_time=SDL_GetTicks();
    page->keyword_dirty=1;
}

/* 回车立即搜索，跳过防抖等待 */
static void packages_page_search_submit(const char *text,void *data){
    packages_page_t *page=data;
    if(page==NULL||page->searching){
        return;
    }

    if(text!=NULL&&strcmp(page->keyword,text)!=0){
        snprintf(page->keyword,sizeof(page->keyword),"%s",text);
    }
    page->change_time=SDL_GetTicks();
    page->keyword_dirty=0;
    packages_page_start_search(page);
}

static void packages_page_slide(float offset,void *data){
    list_set_slide((layout_list_t *)data,offset);
}

static layout_label_t *packages_page_hint(layout_t *parent,const char *text){
    layout_label_t *label=create_layout_label(
        parent,
        1.0f,
        32.0f,
        make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f),
        make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f),
        obtain_font(get_font(TEXT_CONTEXT),get_text_size(TEXT_CONTEXT)),
        text,
        get_text_color(TEXT_CONTEXT)
    );
    if(label!=NULL){
        label->l.enable=0;
    }
    return label;
}

static packages_page_t *create_packages_page(void){
    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    packages_page_t *page=create_layout(
        sizeof(*page),
        NULL,
        0.0f,
        0.0f,
        packages_page_draw,
        packages_page_update,
        NULL,
        NULL,
        zero
    );
    if(page==NULL){
        return NULL;
    }

    page->rect.color=get_color(COLOR_MAIN);

    page->list=create_package_list((layout_t *)&page->rect.l);
    if(page->list==NULL){
        delete_layout((layout_t *)page);
        return NULL;
    }

    page->slider=create_layout_slider(
        (layout_t *)&page->rect.l,
        make_align(1,3,0,1,-get_theme()->slider_width,0.0f,0.0f,1.0f),
        zero,
        0.0f,
        0,
        packages_page_slide,
        page->list
    );
    if(page->slider==NULL){
        delete_layout((layout_t *)page);
        return NULL;
    }

    page->search=widget_create_search_input(
        (layout_t *)&page->rect.l,
        1.0f,
        zero,
        _("Search packages to install"),
        packages_page_search_change,
        page,
        packages_page_search_submit,
        page
    );
    if(page->search==NULL){
        delete_layout((layout_t *)page);
        return NULL;
    }
    widget_input_set_max_bytes(page->search,sizeof(page->keyword)-1);

    page->intro_hint=packages_page_hint((layout_t *)&page->rect.l,
        _("Type a package name in the search box"));
    page->searching_hint=packages_page_hint((layout_t *)&page->rect.l,
        _("Searching packages..."));
    page->empty_hint=packages_page_hint((layout_t *)&page->rect.l,
        _("No matching packages"));
    if(page->intro_hint==NULL||page->searching_hint==NULL||page->empty_hint==NULL){
        delete_layout((layout_t *)page);
        return NULL;
    }

    SDL_SetAtomicInt(&page->results_ready,0);
    packages_page_sync_hints(page);
    return page;
}

static void packages_page_stop(packages_page_t *page){
    if(page==NULL){
        return;
    }
    if(page->searching){
        pthread_join(page->thread,NULL);
        page->searching=0;
    }
}

/* 包操作完成后重扫当前搜索结果：可安装注册表本身没变，只是已安装
 * 状态/版本可能变了。搜索线程还在写 pkg_available 时不能动列表，
 * 等搜索结束时 finish_search 会重新 mark_installed，所以直接跳过。 */
int init_ui_refresh_available_packages(void){
    packages_page_t *page=packages_page_view;
    if(page==NULL||page->list==NULL||page->searching){
        return 0;
    }

    list_clear(page->list);
    list_set_slide(page->list,0.0f);
    page->scan_index=0;
    page->scan_total=pacman_available_count();
    page->scan_running=page->scan_total>0?1u:0u;
    packages_page_mark_installed();
    packages_page_sync_hints(page);
    if(page->slider!=NULL){
        page->slider->all_height=0.0f;
        widget_slider_set(page->slider,0.0f);
    }
    return 1;
}

typedef struct {
    layout_rect_t rect;
    layout_list_t *list;
    widget_slider_t *slider;
    widget_input_t *search;
    layout_label_t *empty_hint;

    char filter[128];      /* 当前过滤词，空串表示不过滤 */
    size_t scan_index;     /* 已经扫过的包下标，卡片由 scan 逐步补进列表 */
    float header_offset;   /* selector 占用的高度，搜索框从这里开始 */

    unsigned filter_dirty:1;
    unsigned scan_running:1;
}installed_page_t;

/* 供异步加载完成后的刷新函数使用；UI 销毁前清空。 */
static installed_page_t *installed_page_view=NULL;

/* 已枚举（可以生成卡片）的包数和总数；由 begin/batch 推进，页面负责按帧补卡片。 */
static size_t package_refresh_index=0;
static size_t package_refresh_total=0;

/* 不区分 ASCII 大小写的子串匹配；filter 为空时全部命中。 */
static int package_name_matches(const package_t *package,const char *filter){
    if(package==NULL||package->name==NULL){
        return 0;
    }
    if(filter==NULL||filter[0]=='\0'){
        return 1;
    }

    for(size_t i=0;package->name[i]!='\0';i++){
        size_t j=0;
        while(filter[j]!='\0'&&package->name[i+j]!='\0'&&
              tolower((unsigned char)package->name[i+j])==
              tolower((unsigned char)filter[j])){
            j++;
        }
        if(filter[j]=='\0'){
            return 1;
        }
    }
    return 0;
}

static void installed_page_add_card(installed_page_t *page,size_t index){
    package_t *package=pacman_package_at(index);
    if(package==NULL){
        return;
    }

    layout_rect_t *card=create_layout_package(package);
    if(card==NULL||!list_add(page->list,(layout_t *)card)){
        if(card!=NULL){
            delete_layout((layout_t *)card);
        }
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to create package card %zu"),index);
    }
}

/* 把已枚举的包按当前过滤词补进列表：
 * - 每帧最多生成 PAGE_CARDS_PER_FRAME 张卡片，输入时不卡顿；
 * - 不匹配的包只做字符串比较，不占预算。 */
static void installed_page_scan(installed_page_t *page){
    if(page==NULL||page->list==NULL){
        return;
    }
    if(page->scan_index<package_refresh_index){
        page->scan_running=1;
    }
    if(!page->scan_running){
        return;
    }

    size_t budget=PAGE_CARDS_PER_FRAME;
    while(page->scan_index<package_refresh_index&&budget>0){
        size_t index=page->scan_index++;
        package_t *package=pacman_package_at(index);
        if(package==NULL||!package_name_matches(package,page->filter)){
            continue;
        }
        budget--;
        installed_page_add_card(page,index);
    }

    if(page->scan_index>=package_refresh_index){
        page->scan_running=0;
    }
}

static void installed_page_sync_empty(installed_page_t *page){
    if(page==NULL||page->empty_hint==NULL){
        return;
    }

    int filtering=page->filter[0]!='\0';
    int empty=page->list==NULL||page->list->sons.size==0;
    page->empty_hint->l.enable=(filtering&&empty&&!page->scan_running)?1:0;
}

static void installed_page_draw(void *p){
    installed_page_t *page=p;
    if(page!=NULL){
        rect_draw(&page->rect);
    }
}

static void installed_page_update(void *p){
    installed_page_t *page=p;
    if(page==NULL){
        return;
    }

    layout_update(&page->rect.l);

    /* 过滤词变化时清空列表重新扫描；重建放在这里而不是回调里，
     * 避免在遍历子布局的过程中删除列表项。 */
    if(page->filter_dirty){
        page->filter_dirty=0;
        if(page->list!=NULL){
            list_clear(page->list);
            list_set_slide(page->list,0.0f);
        }
        page->scan_index=0;
        page->scan_running=1;
    }

    installed_page_scan(page);
    installed_page_sync_empty(page);

    float width=page->rect.l.width;
    if(width>0.0f){
        if(page->search!=NULL){
            page->search->l.x=page->rect.l.x+PAGE_MARGIN;
            page->search->l.y=page->rect.l.y+page->header_offset;
            page->search->l.width=width-PAGE_MARGIN*2.0f;
        }
        if(page->empty_hint!=NULL&&page->search!=NULL){
            page->empty_hint->l.x=page->rect.l.x+PAGE_MARGIN;
            page->empty_hint->l.y=page->search->l.y+page->search->l.height+32.0f;
            page->empty_hint->l.width=width-PAGE_MARGIN*2.0f;
            page->empty_hint->l.height=32.0f;
        }
    }

    if(page->list!=NULL&&page->slider!=NULL){
        page->slider->all_height=list_get_content_height(page->list);
        widget_slider_set(page->slider,list_get_slide(page->list));
    }
}

/* 搜索框文本变化：只记过滤词和脏标记，重建在 installed_page_update() 里做。 */
static void installed_page_search_change(const char *text,void *data){
    installed_page_t *page=data;
    if(page==NULL){
        return;
    }

    if(text==NULL){
        text="";
    }
    if(strcmp(page->filter,text)==0){
        return;
    }

    snprintf(page->filter,sizeof(page->filter),"%s",text);
    page->filter_dirty=1;
}

static void installed_page_slide(float offset,void *data){
    list_set_slide((layout_list_t *)data,offset);
}

static installed_page_t *create_installed_page(void){
    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    installed_page_t *page=create_layout(
        sizeof(*page),
        NULL,
        0.0f,
        0.0f,
        installed_page_draw,
        installed_page_update,
        NULL,
        NULL,
        zero
    );
    if(page==NULL){
        return NULL;
    }

    page->rect.color=get_color(COLOR_MAIN);

    page->list=create_package_list((layout_t *)&page->rect.l);
    if(page->list==NULL){
        delete_layout((layout_t *)page);
        return NULL;
    }

    page->slider=create_layout_slider(
        (layout_t *)&page->rect.l,
        make_align(1,3,0,1,-get_theme()->slider_width,0.0f,0.0f,1.0f),
        zero,
        list_get_content_height(page->list),
        0,
        installed_page_slide,
        page->list
    );
    if(page->slider==NULL){
        delete_layout((layout_t *)page);
        return NULL;
    }

    /* 搜索框和空状态提示在列表之后创建，绘制时会盖住滚动到顶部的卡片 */
    page->search=widget_create_search_input(
        (layout_t *)&page->rect.l,
        1.0f,
        zero,
        _("Search installed packages"),
        installed_page_search_change,
        page,
        NULL,
        NULL
    );
    if(page->search==NULL){
        delete_layout((layout_t *)page);
        return NULL;
    }
    widget_input_set_max_bytes(page->search,sizeof(page->filter)-1);

    page->empty_hint=create_layout_label(
        (layout_t *)&page->rect.l,
        1.0f,
        32.0f,
        zero,
        make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f),
        obtain_font(get_font(TEXT_CONTEXT),get_text_size(TEXT_CONTEXT)),
        _("No matching packages"),
        get_text_color(TEXT_CONTEXT)
    );
    if(page->empty_hint==NULL){
        delete_layout((layout_t *)page);
        return NULL;
    }
    page->empty_hint->l.enable=0;

    return page;
}

static void main_tabs_size(float *w,float *h){
    *w=get_root_w();
    *h=get_root_h();
}

int init_ui_begin_refresh_packages(void){
    installed_page_t *page=installed_page_view;
    if(page==NULL||page->list==NULL){
        return 0;
    }

    /* 调用方负责保证此时没有后台线程再写 pkg_installed（例如先 join）。 */
    list_clear(page->list);
    list_set_slide(page->list,0.0f);
    package_refresh_index=0;
    package_refresh_total=pacman_package_count();
    page->scan_index=0;
    page->scan_running=1;
    if(page->slider!=NULL){
        page->slider->all_height=0.0f;
        widget_slider_set(page->slider,0.0f);
    }
    installed_page_sync_empty(page);
    return 1;
}

int init_ui_refresh_packages_batch(size_t max_items){
    installed_page_t *page=installed_page_view;
    if(page==NULL||page->list==NULL){
        return 0;
    }
    if(max_items==0){
        max_items=1;
    }

    /* 这里只推进“已枚举”游标；卡片由 installed_page_update() 按帧限额补进列表，
     * 过滤和后台枚举因此共用同一套增量逻辑。 */
    if(package_refresh_index<package_refresh_total){
        size_t remaining=package_refresh_total-package_refresh_index;
        package_refresh_index+=remaining<max_items?remaining:max_items;
    }
    return package_refresh_index<package_refresh_total?1:0;
}

void init_ui_stop_threads(void){
    /* 必须在 sdlgui_quit() 删除布局之前 join，因为页面指针随后会失效。 */
    packages_page_stop(packages_page_view);
}

void init_ui_quit(void){
    packages_page_view=NULL;
    installed_page_view=NULL;
    pacman_quit_all();
}

/* packages 和 installed 页顶部都浮着 selector，下面接一行搜索框：
 * 搜索框从 selector 下缘开始，列表内容再给两者让位；末尾也留出 selector 高度。 */
static void page_apply_header(layout_rect_t *rect,float *header_offset,
    widget_input_t *search,layout_list_t *list,float selector_height){
    if(rect==NULL){
        return;
    }

    if(header_offset!=NULL&&search!=NULL){
        *header_offset=selector_height+PAGE_SEARCH_GAP;
    }
    if(list!=NULL){
        float inset=selector_height;
        if(search!=NULL){
            inset+=PAGE_SEARCH_GAP+search->l.height+PAGE_SEARCH_GAP;
        }
        list->front_offset=inset;
        list->back_offset=selector_height;
    }
}

int init_ui(void){
    update_root();

    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);

    /* packages 页搜索可安装包；installed 页读取已安装包。 */
    packages_page_t *packages_page=create_packages_page();
    installed_page_t *installed_page=create_installed_page();
    layout_rect_t *settings_page=create_layout_rect(NULL,0.0f,0.0f,zero,get_color(COLOR_MAIN));

    widget_selector_t *selector=widget_create_selector_title(
        get_root_layout(),
        make_align(3,0,0,0,0.0f,0.0f,0.0f,0.0f),
        get_root_w()/4.0f,
        NULL,
        NULL,
        "\\package package",
        "\\installed installed",
        "\\settings settings",
        NULL
    );

    widget_tabs_t *tabs=widget_create_tabs(
        get_root_layout(),
        zero,
        main_tabs_size,
        selector
    );

    if(tabs==NULL||selector==NULL||
        packages_page==NULL||installed_page==NULL||settings_page==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to create main tabs"));
        packages_page_view=NULL;
        installed_page_view=NULL;
        return 0;
    }
    packages_page_view=packages_page;
    installed_page_view=installed_page;

    float selector_height=selector->l.l.height;
    page_apply_header(&packages_page->rect,&packages_page->header_offset,
        packages_page->search,packages_page->list,selector_height);
    page_apply_header(&installed_page->rect,&installed_page->header_offset,
        installed_page->search,installed_page->list,selector_height);

    layout_t *pages[]={
        (layout_t *)&packages_page->rect.l,
        (layout_t *)&installed_page->rect.l,
        (layout_t *)settings_page
    };
    widget_set_tabs(tabs,pages,3);
    update_root();
    return 1;
}
