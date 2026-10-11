/*
 * main.c —— SDL3 空白窗口模板
 *
 * 编译：make        运行：make run
 *
 * 目前只做三件事：开窗口、清屏、响应退出。
 * 界面和逻辑按注释里的 TODO 往下加即可。
 */
#include <SDL3/SDL.h>
#include "sdlgui.h"

#define WINDOW_TITLE "pacman-gui"
#define WINDOW_W 640
#define WINDOW_H 480

#define TRUE 1
#define FALSE 0


int main(int argc,char *argv[]){
    (void)argc;
    (void)argv;

    sdlgui_init("pacman-gui",640,480,"assets/icon-64.png");

    rect_layout_t *rect=create_layout(sizeof(rect_layout_t),NULL,0,45,draw_rect,update_layout,NULL,make_align(0,0,1,0,0,0,1.0f,0));
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
            }
            /* TODO: 这里处理鼠标点击、键盘输入等事件 */
        }

        /* TODO: 这里按帧更新程序状态 */

        /* 3.2 绘制：清屏 -> 画界面 -> 提交 */
        SDL_SetRenderDrawColor(get_renderer(),0,0,0,255);
        SDL_RenderClear(get_renderer());

        update_root(); /* 重算几何并绘制（all 现在等于窗口大小） */

        SDL_RenderPresent(get_renderer());
    }

    sdlgui_quit();
    return 0;
}
