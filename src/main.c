#include <SDL3/SDL.h>
#include <pthread.h>
#include "sdlgui.h"
#include "sdlgui_i18n.h"
#include "pacman.h"
#include "init.h"

#define WINDOW_TITLE "pacman-gui"
#define WINDOW_W 640
#define WINDOW_H 480
#define WINDOW_MIN_W 400
#define WINDOW_MIN_H 360

#define TRUE 1
#define FALSE 0

/* 每帧最多创建的 installed 卡片数：每加完这一批就回到事件循环，
 * 用户不必等所有卡片一次性建完。 */
#define PACKAGE_UI_BATCH 32

/* 后台枚举线程完成后置 1；主线程据此 join 并刷新 installed 页。 */
static SDL_AtomicInt packages_ready;

static void *load_installed_packages(void *data){
    (void)data;
    pacman_list_installed_all();
    SDL_SetAtomicInt(&packages_ready,1);
    return NULL;
}

int main(int argc,char *argv[]){
    (void)argc;
    (void)argv;

    /* 应用名/版本/标识符；macOS 的菜单栏和包信息会用到 */
    SDL_SetAppMetadata(WINDOW_TITLE,"0.0.1","io.github.lupupulu.pacman-gui");

    /* 包管理器先于 SDL 和窗口初始化；这里只打开后端，不枚举软件包。
     * 枚举放到 init_ui() 之后的后台线程，避免阻塞窗口出现。 */
    int backends_initialized=pacman_init_all();

    sdlgui_init(WINDOW_TITLE,WINDOW_W,WINDOW_H,"assets/icon-64.png",NULL);

    if(backends_initialized<=0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("No package backend initialized"));
    }

    SDL_Window *window=get_window();
    if(window==NULL||!SDL_SetWindowMinimumSize(window,WINDOW_MIN_W,WINDOW_MIN_H)){
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    _("Failed to set minimum window size: %s"),SDL_GetError());
    }

    if(!init_ui()){
        sdlgui_quit();
        init_ui_quit();
        return 1;
    }

    /* macOS 的交互式缩放会阻塞主循环，需要事件 watch 里同步重绘；
     * Linux/Windows 的合成器异步处理 resize，主循环每帧都会渲染。
     * 在事件 watch 里额外 present 可能在窗口尺寸真正生效前提交一张
     * 旧尺寸的帧，拖动缩放时就会被合成器拉伸成虚影。 */
#ifdef __APPLE__
    sdlgui_enable_live_resize();
#endif

    /* UI 已经就绪：在后台线程枚举所有后端的已安装包。主循环只轮询完成
     * 标志；完成后 join，再按 PACKAGE_UI_BATCH 每帧追加一批卡片，保持界面
     * 在枚举和建卡期间都可交互。 */
    SDL_SetAtomicInt(&packages_ready,0);
    pthread_t loader;
    int loader_started=pthread_create(&loader,NULL,load_installed_packages,NULL)==0;
    int loader_done=0;
    int packages_refreshing=0;
    if(!loader_started){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("Failed to create the package loader thread"));
        pacman_list_installed_all();
        loader_done=1;
        if(init_ui_begin_refresh_packages()){
            packages_refreshing=1;
        }else{
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("Failed to refresh the installed package list"));
        }
    }

    int running=TRUE;
    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(!sdlgui_handle_event(&ev)){
                running=FALSE;
            }
        }

        if(!loader_done&&SDL_GetAtomicInt(&packages_ready)){
            int join_error=pthread_join(loader,NULL);
            loader_started=0;
            loader_done=1;
            if(join_error!=0){
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             _("Failed to join the package loader thread: %d"),join_error);
            }
            if(init_ui_begin_refresh_packages()){
                packages_refreshing=1;
            }else{
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             _("Failed to refresh the installed package list"));
            }
        }

        if(packages_refreshing&&!init_ui_refresh_packages_batch(PACKAGE_UI_BATCH)){
            packages_refreshing=0;
        }

        sdlgui_render_frame();
    }

    /* 退出前等待枚举线程，之后才能安全释放 pkg_installed。 */
    if(loader_started){
        pthread_join(loader,NULL);
        loader_started=0;
    }

#ifdef __APPLE__
    sdlgui_disable_live_resize();
#endif

    sdlgui_quit();
    init_ui_quit();
    return 0;
}
