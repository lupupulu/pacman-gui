/*
 * main.c —— SDL3 空白窗口模板
 *
 * 编译：make        运行：make run
 *
 * 目前只做三件事：开窗口、清屏、响应退出。
 * 界面和逻辑按注释里的 TODO 往下加即可。
 */
#include <stdbool.h>

#include <SDL3/SDL.h>

#define WINDOW_TITLE "pacman-gui"
#define WINDOW_W 1024
#define WINDOW_H 720

#define TRUE 1
#define FALSE 0

int main(int argc,char *argv[]){
    (void)argc;
    (void)argv;

    /* ---- 1. 初始化 SDL ---- */
    if(!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init 失败: %s", SDL_GetError());
        return 1;
    }

    /* ---- 2. 创建窗口与渲染器（SDL3 中两者都是不透明指针） ---- */
    SDL_Window *window=NULL;
    SDL_Renderer *renderer=NULL;

    if(!SDL_CreateWindowAndRenderer(WINDOW_TITLE,WINDOW_W,WINDOW_H,SDL_WINDOW_RESIZABLE,&window,&renderer)){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"创建窗口/渲染器失败: %s",SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderVSync(renderer,1); /* 开垂直同步，避免主循环空转烧 CPU */

    /* ---- 3. 主循环 ---- */
    int running=TRUE;

    while(running){
        /* 3.1 事件 */
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_EVENT_QUIT){
                running=FALSE;
            }else if(ev.type==SDL_EVENT_KEY_DOWN&&ev.key.key==SDLK_ESCAPE){
                running=FALSE;
            }
            /* TODO: 这里处理鼠标点击、键盘输入、窗口缩放等事件 */
        }

        /* TODO: 这里按帧更新程序状态 */

        /* 3.2 绘制：清屏 -> 画界面 -> 提交 */
        SDL_SetRenderDrawColor(renderer,18,18,24,255);
        SDL_RenderClear(renderer);

        /* TODO: 这里绘制界面（文字、列表、按钮……） */

        SDL_RenderPresent(renderer);
    }

    /* ---- 4. 清理（逆序释放） ---- */
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
