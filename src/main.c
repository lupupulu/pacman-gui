#include <SDL3/SDL.h>
#include "sdlgui.h"
#include "sdlgui_i18n.h"
#include "init.h"

#define WINDOW_TITLE "pacman-gui"
#define WINDOW_W 640
#define WINDOW_H 480
#define WINDOW_MIN_W 400
#define WINDOW_MIN_H 360

#define TRUE 1
#define FALSE 0

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

    if(!init_ui()){
        sdlgui_quit();
        return 1;
    }

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
