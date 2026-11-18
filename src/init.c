#include <SDL3/SDL.h>
#include "sdlgui.h"
#include "sdlgui_widget.h"
#include "sdlgui_i18n.h"
#include "pacman_gui.h"
#include "pacman.h"
#include "init.h"

/* 按当前 pkg_installed 内容生成卡片；初始化时列表为空，后台枚举完成后
 * 由 init_ui_begin_refresh_packages()/init_ui_refresh_packages_batch()
 * 分批补齐。 */
static void package_list_fill(layout_list_t *list){
    size_t count=pacman_package_count();
    for(size_t i=0;i<count;i++){
        package_t *package=pacman_package_at(i);
        if(package==NULL){
            continue;
        }
        layout_rect_t *card=create_layout_package(package);
        if(card==NULL||!list_add(list,(layout_t *)card)){
            if(card!=NULL){
                delete_layout((layout_t *)card);
            }
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to create package card %zu"),i);
        }
    }
}

/* 通过 pacman.c 的统一包视图生成 installed 列表。 */
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
    package_list_fill(list);
    return list;
}

typedef struct {
    layout_rect_t rect;
    layout_list_t *list;
    widget_slider_t *slider;
}installed_page_t;

/* 供异步加载完成后的刷新函数使用；UI 销毁前清空。 */
static installed_page_t *installed_page_view=NULL;

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
    if(page->list!=NULL&&page->slider!=NULL){
        page->slider->all_height=list_get_content_height(page->list);
        widget_slider_set(page->slider,list_get_slide(page->list));
    }
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

    return page;
}

static void main_tabs_size(float *w,float *h){
    *w=get_root_w();
    *h=get_root_h();
}

/* 增量刷新的游标；begin 时记录一次总包数，batch 只往后追加卡片。 */
static size_t package_refresh_index=0;
static size_t package_refresh_total=0;

int init_ui_begin_refresh_packages(void){
    installed_page_t *page=installed_page_view;
    if(page==NULL||page->list==NULL){
        return 0;
    }

    /* 调用方负责保证此时没有后台线程再写 pkg_installed（例如先 join）。 */
    list_clear(page->list);
    package_refresh_index=0;
    package_refresh_total=pacman_package_count();
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

    size_t added=0;
    while(package_refresh_index<package_refresh_total&&added<max_items){
        size_t index=package_refresh_index++;
        package_t *package=pacman_package_at(index);
        if(package==NULL){
            continue;
        }
        layout_rect_t *card=create_layout_package(package);
        if(card==NULL||!list_add(page->list,(layout_t *)card)){
            if(card!=NULL){
                delete_layout((layout_t *)card);
            }
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to create package card %zu"),index);
        }
        added++;
    }

    /* 保持用户当前滚动位置，只更新内容总高度。 */
    if(page->slider!=NULL){
        float slide=list_get_slide(page->list);
        page->slider->all_height=list_get_content_height(page->list);
        widget_slider_set(page->slider,slide);
    }

    return package_refresh_index<package_refresh_total?1:0;
}

void init_ui_quit(void){
    installed_page_view=NULL;
    pacman_quit_all();
}

int init_ui(void){
    update_root();

    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);

    /* package 页暂时留空；installed 页通过 pacman.c 的统一包视图读取。 */
    layout_rect_t *package_page=create_layout_rect(NULL,0.0f,0.0f,zero,get_color(COLOR_MAIN));
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
        package_page==NULL||installed_page==NULL||settings_page==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to create main tabs"));
        installed_page_view=NULL;
        return 0;
    }
    installed_page_view=installed_page;

    /* installed 列表前后各留出 selector 的高度：第一个 item 从 selector
     * 下缘开始，滚动时可以从 selector 底下经过；末尾留出同样的空间。 */
    if(installed_page->list!=NULL){
        float inset=selector->l.l.height;
        installed_page->list->front_offset=inset;
        installed_page->list->back_offset=inset;
    }

    layout_t *pages[]={
        (layout_t *)package_page,
        (layout_t *)&installed_page->rect.l,
        (layout_t *)settings_page
    };
    widget_set_tabs(tabs,pages,3);
    update_root();
    return 1;
}
