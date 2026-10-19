#include <SDL3/SDL.h>
#include "sdlgui.h"

#define WINDOW_TITLE "pacman-gui"
#define WINDOW_W 640
#define WINDOW_H 480

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

    /* 按下就用按下时的坐标，回弹就用回弹时的坐标 */
    set_mouse_pos(ev->button.x,ev->button.y);

    /* 事件本身只带"哪个键变了"，整张掩码要现查一次（同一批 pump 的状态，是准的） */
    SDL_MouseButtonFlags m=SDL_GetMouseState(NULL,NULL);
    set_mouse_button((m&SDL_BUTTON_LMASK)!=0,
                     (m&SDL_BUTTON_MMASK)!=0,
                     (m&SDL_BUTTON_RMASK)!=0,
                     (m&SDL_BUTTON_X1MASK)!=0,
                     (m&SDL_BUTTON_X2MASK)!=0);
}


int main(int argc,char *argv[]){
    (void)argc;
    (void)argv;

    sdlgui_init("pacman-gui",640,480,"assets/icon-64.png");

    layout_rect_t *rect=create_layout_rect(NULL,0,45,make_align(0,0,1,0,0,0,1.0f,0));
    rect->color=0xFFFFFFFF;
    rect->l.enable=1;


    int running=TRUE;

    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_EVENT_QUIT){
                running=FALSE;
            }else if(ev.type==SDL_EVENT_KEY_DOWN&&ev.key.key==SDLK_ESCAPE){
                running=FALSE;
            }else if(ev.type==SDL_EVENT_WINDOW_RESIZED
                  || ev.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED){
                update_root();
            }else if(ev.type==SDL_EVENT_MOUSE_MOTION){
                update_mouse(&ev);
            }else if(ev.type==SDL_EVENT_MOUSE_BUTTON_DOWN
                  || ev.type==SDL_EVENT_MOUSE_BUTTON_UP){
                update_mouse_button(&ev);
            }
            /* TODO: 这里处理鼠标点击、键盘输入等事件 */
        }

        /* TODO: 这里按帧更新程序状态 */

        /* 3.2 绘制：清屏 -> 画界面 -> 提交 */
        SDL_SetRenderDrawColor(get_renderer(),0,0,0,255);
        SDL_RenderClear(get_renderer());

        update_root();

        SDL_RenderPresent(get_renderer());
    }

    sdlgui_quit();
    return 0;
}
