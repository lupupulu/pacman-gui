#include <SDL3/SDL.h>
#include "sdlgui.h"
#include "sdlgui_widget.h"
#include "sdlgui_i18n.h"
#include "pacman_gui.h"
#include "pacman.h"
#include "init.h"

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
    return list;
}

typedef struct {
    layout_rect_t rect;
    layout_list_t *list;
    widget_slider_t *slider;
}installed_page_t;

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

void init_ui_quit(void){
    pacman_quit_all();
}

int init_ui(void){
    update_root();
    if(pacman_init_all()<=0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("No package backend initialized"));
    }

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
        return 0;
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
