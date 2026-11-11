#include <SDL3/SDL.h>
#include "sdlgui.h"
#include "sdlgui_widget.h"
#include "sdlgui_i18n.h"
#include "pacman_gui.h"

#define WINDOW_TITLE "pacman-gui"
#define WINDOW_W 640
#define WINDOW_H 480
#define WINDOW_MIN_W 400
#define WINDOW_MIN_H 360

#define TRUE 1
#define FALSE 0

/* 仅用于绘制测试的软件包数据；安装/删除/更新逻辑尚未实现。 */
static layout_list_t *create_test_package_list(layout_t *parent){
    static char *maintainers_alice[]={"Alice",NULL};
    static char *maintainers_carol[]={"Carol",NULL};
    static char *maintainers_mozilla[]={"Mozilla",NULL};
    static package_t packages[]={
        {
            .name="pacman",
            .version="6.1.0",
            .license="GPL-2.0",
            .submitter="Bob",
            .maintainers=maintainers_alice,
            .installed=1
        },
        {
            .name="zlib",
            .version="1.3.1",
            .license="Zlib",
            .submitter=NULL,
            .maintainers=NULL,
            .installed=0
        },
        {
            .name="neovim",
            .version="0.10.2",
            .license="Apache-2.0",
            .submitter="Dave",
            .maintainers=maintainers_carol,
            .installed=0
        },
        {
            .name="firefox",
            .version="132.0",
            .license="MPL-2.0",
            .submitter="Eve",
            .maintainers=maintainers_mozilla,
            .installed=1
        },
        {
            .name="htop",
            .version="3.3.0",
            .license="GPL-2.0",
            .submitter="Grace",
            .maintainers=maintainers_carol,
            .installed=0
        },
        {
            .name="ripgrep",
            .version="14.1.1",
            .license="MIT",
            .submitter="Heidi",
            .maintainers=maintainers_alice,
            .installed=0
        }
    };

    size_t count=sizeof(packages)/sizeof(packages[0]);
    layout_rect_t *cards[sizeof(packages)/sizeof(packages[0])];
    for(size_t i=0;i<count;i++){
        cards[i]=create_layout_package(&packages[i]);
        if(cards[i]==NULL){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("Failed to create package card %zu"),i);
            return NULL;
        }
    }

    layout_list_t *list=create_layout_list(
        parent,
        0.0f,
        0.0f,
        make_align(0,0,1,1,0.0f,0.0f,1.0f,1.0f),
        0,
        (layout_t *)cards[0],
        (layout_t *)cards[1],
        (layout_t *)cards[2],
        (layout_t *)cards[3],
        (layout_t *)cards[4],
        (layout_t *)cards[5],
        NULL
    );
    if(list==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to create package list"));
    }
    return list;
}

static void main_tabs_size(float *w,float *h){
    *w=get_root_w();
    *h=get_root_h();
}

int main(int argc,char *argv[]){
    (void)argc;
    (void)argv;

    /* 应用名/版本/标识符；macOS 的菜单栏和包信息会用到 */
    SDL_SetAppMetadata(WINDOW_TITLE,"0.0.1","io.github.lupupulu.pacman-gui");

    sdlgui_init(WINDOW_TITLE,WINDOW_W,WINDOW_H,"assets/icon-64.png",NULL);

    SDL_Window *window=get_window();
    if(window==NULL||!SDL_SetWindowMinimumSize(window,WINDOW_MIN_W,WINDOW_MIN_H)){
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    _("Failed to set minimum window size: %s"),SDL_GetError());
    }

    update_root();

    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);

    /* 第一个页面直接就是 package 列表，避免多一层容器导致 y 偏移丢失。 */
    layout_list_t *package_list=create_test_package_list(get_root_layout());

    /* 另外两个页面暂时只画空矩形。 */
    layout_rect_t *installed_page=create_layout_rect(NULL,0.0f,0.0f,zero);
    layout_rect_t *settings_page=create_layout_rect(NULL,0.0f,0.0f,zero);
    if(installed_page!=NULL){
        installed_page->color=get_color(COLOR_SECONDARY);
    }
    if(settings_page!=NULL){
        settings_page->color=get_color(COLOR_SECONDARY);
    }

    widget_selector_t *selector=widget_create_selector_title(
        NULL,
        zero,
        get_root_w()/3.0f,
        NULL,
        NULL,
        "\\package package",
        "\\installed installed",
        "\\settings settings",
        NULL
    );
    if(selector!=NULL){
        selector->l.l.x=0.0f;
        selector->l.l.y=0.0f;
    }

    widget_tabs_t *tabs=widget_create_tabs(
        get_root_layout(),
        zero,
        main_tabs_size,
        selector
    );
    if(tabs==NULL||selector==NULL||package_list==NULL||installed_page==NULL||settings_page==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to create main tabs"));
    }else{
        layout_t *pages[]={
            (layout_t *)package_list,
            (layout_t *)installed_page,
            (layout_t *)settings_page
        };
        widget_set_tabs(tabs,pages,3);
    }
    update_root();

    sdlgui_enable_live_resize();

    int running=TRUE;
    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(!sdlgui_handle_event(&ev)){
                running=FALSE;
            }
        }

        sdlgui_render_frame();
    }

    sdlgui_disable_live_resize();

    sdlgui_quit();
    return 0;
}
