#include <SDL3/SDL.h>
#include "sdlgui.h"
#include "sdlgui_widget.h"
#include "sdlgui_i18n.h"
#include "pacman_gui.h"
#include "init.h"

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
            .stat=PKG_STAT_INSTALLED
        },
        {
            .name="zlib",
            .version="1.3.1",
            .license="Zlib",
            .submitter=NULL,
            .maintainers=NULL,
            .stat=PKG_STAT_NULL
        },
        {
            .name="neovim",
            .version="0.10.2",
            .license="Apache-2.0",
            .submitter="Dave",
            .maintainers=maintainers_carol,
            .stat=PKG_STAT_NULL
        },
        {
            .name="firefox",
            .version="132.0",
            .license="MPL-2.0",
            .submitter="Eve",
            .maintainers=maintainers_mozilla,
            .stat=PKG_STAT_INSTALLED
        },
        {
            .name="htop",
            .version="3.3.0",
            .license="GPL-2.0",
            .submitter="Grace",
            .maintainers=maintainers_carol,
            .stat=PKG_STAT_NULL
        },
        {
            .name="ripgrep",
            .version="14.1.1",
            .license="MIT",
            .submitter="Heidi",
            .maintainers=maintainers_alice,
            .stat=PKG_STAT_NULL
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

int init_ui(void){
    update_root();

    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);

    /* 第一个页面直接就是 package 列表，避免多一层容器导致 y 偏移丢失。 */
    layout_list_t *package_list=create_test_package_list(get_root_layout());

    /* 另外两个页面暂时只画空矩形。 */
    layout_rect_t *installed_page=create_layout_rect(NULL,0.0f,0.0f,zero,get_color(COLOR_MAIN));
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
        package_list==NULL||installed_page==NULL||settings_page==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to create main tabs"));
        return 0;
    }

    layout_t *pages[]={
        (layout_t *)package_list,
        (layout_t *)installed_page,
        (layout_t *)settings_page
    };
    widget_set_tabs(tabs,pages,3);
    update_root();
    return 1;
}
