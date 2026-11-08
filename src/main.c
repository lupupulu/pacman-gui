#include <SDL3/SDL.h>
#include "sdlgui.h"
#include "sdlgui_widget.h"
#include "sdlgui_i18n.h"

#define WINDOW_TITLE "pacman-gui"
#define WINDOW_W 640
#define WINDOW_H 480
#define WINDOW_MIN_W 400
#define WINDOW_MIN_H 360

#define TRUE 1
#define FALSE 0

void update_mouse(const SDL_Event *ev){
    if(ev->type!=SDL_EVENT_MOUSE_MOTION){
        return;
    }
    if(get_window()==NULL||ev->motion.windowID!=SDL_GetWindowID(get_window())){
        return;   /* 不是本窗口的移动，不更新 */
    }
    set_mouse_pos(ev->motion.x,ev->motion.y);
}

/* 按下/回弹时把五个按键状态交给 gui，坐标各用各的事件坐标 */
void update_mouse_button(const SDL_Event *ev){
    if(ev->type!=SDL_EVENT_MOUSE_BUTTON_DOWN&&ev->type!=SDL_EVENT_MOUSE_BUTTON_UP){
        return;
    }
    if(get_window()==NULL||ev->button.windowID!=SDL_GetWindowID(get_window())){
        return;   /* 不是本窗口的按键 */
    }

    set_mouse_pos(ev->button.x,ev->button.y);

    SDL_MouseButtonFlags m=SDL_GetMouseState(NULL,NULL);
    set_mouse_button((m&SDL_BUTTON_LMASK)!=0,
                     (m&SDL_BUTTON_MMASK)!=0,
                     (m&SDL_BUTTON_RMASK)!=0,
                     (m&SDL_BUTTON_X1MASK)!=0,
                     (m&SDL_BUTTON_X2MASK)!=0);
}

/* 滚轮和触摸板滚动都走 SDL_EVENT_MOUSE_WHEEL，y 正方向是滚轮向上 */
void update_mouse_wheel(const SDL_Event *ev){
    if(ev->type!=SDL_EVENT_MOUSE_WHEEL){
        return;
    }
    if(get_window()==NULL||ev->wheel.windowID!=SDL_GetWindowID(get_window())){
        return;   /* 不是本窗口的滚轮 */
    }
    set_mouse_pos(ev->wheel.mouse_x,ev->wheel.mouse_y);
    set_mouse_wheel(ev->wheel.x,ev->wheel.y);
}

static void selector_callback(size_t now,size_t from,void *data){
    (void)data;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                _("Selector changed: %zu -> %zu"),from,now);
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

    /* 顶部工具栏：铺满窗口宽度的矩形，选择器在其中居中 */
    layout_rect_t *top_bar=create_layout_rect(
        get_root_layout(),0.0f,64.0f,make_align(0,0,1,0,0.0f,0.0f,1.0f,0.0f));
    if(top_bar!=NULL){
        top_bar->color=get_color(COLOR_SECONDARY);
    }
    update_root();

    widget_selector_t *selector=widget_create_selector_title(
        top_bar!=NULL?(void *)top_bar:get_root_layout(),
        make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f),
        125.0f,
        selector_callback,
        NULL,
        "\\package package",
        "\\installed installed",
        "\\settings settings",
        NULL);
    if(top_bar==NULL||selector==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to create top bar or selector"));
    }

    update_root();

    int running=TRUE;
    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_EVENT_QUIT){
                running=FALSE;
            }else if(ev.type==SDL_EVENT_KEY_DOWN&&ev.key.key==SDLK_ESCAPE){
                running=FALSE;
            }else if(ev.type==SDL_EVENT_WINDOW_RESIZED
                  || ev.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
                  || ev.type==SDL_EVENT_WINDOW_DISPLAY_CHANGED){
                update_root();
            }else if(ev.type==SDL_EVENT_SYSTEM_THEME_CHANGED){
                theme_select_system();
            }else if(ev.type==SDL_EVENT_MOUSE_MOTION){
                update_mouse(&ev);
            }else if(ev.type==SDL_EVENT_MOUSE_BUTTON_DOWN
                  || ev.type==SDL_EVENT_MOUSE_BUTTON_UP){
                update_mouse_button(&ev);
            }else if(ev.type==SDL_EVENT_MOUSE_WHEEL){
                update_mouse_wheel(&ev);
            }
        }

        unsigned background=get_color(COLOR_MAIN);
        SDL_SetRenderDrawColor(get_renderer(),
            (background>>24)&0xFF,(background>>16)&0xFF,
            (background>>8)&0xFF,background&0xFF);
        SDL_RenderClear(get_renderer());

        update_root();
        draw_root();

        SDL_RenderPresent(get_renderer());
    }

    sdlgui_quit();
    return 0;
}
