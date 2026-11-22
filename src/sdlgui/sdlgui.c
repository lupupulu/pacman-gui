/*
 * Copyright (C) 2026 Zhang Cw and pacman-gui contributors
 *
 * This file is part of pacman-gui.
 *
 * pacman-gui is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pacman-gui is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with pacman-gui.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "sdlgui.h"
#include "layout.h"
#include "input.h"
#include "sdlgui_internal.h"
#include "sdlgui_i18n.h"
#include "fontlookup.h"
#include "CDS/confc.h"
#include <locale.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>

#ifdef SDLGUI_FONT_BACKEND_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

#include "CDS/darray.h"

#define TRUE  1
#define FALSE 0

/* ---- 路径工具：主题/资源搜索用 ---- */

static char *gui_join_path(const char *left,const char *right){
    if(left==NULL){ left=""; }
    if(right==NULL){ right=""; }

    size_t left_size=strlen(left);
    size_t right_size=strlen(right);
    int slash=left_size>0&&left[left_size-1]!='/';

    char *result=malloc(left_size+(size_t)slash+right_size+1);
    if(result==NULL){
        return NULL;
    }

    memcpy(result,left,left_size);
    size_t pos=left_size;
    if(slash){
        result[pos++]='/';
    }
    memcpy(result+pos,right,right_size+1);
    return result;
}

static int gui_path_readable(const char *path){
    return path!=NULL&&path[0]!='\0'&&access(path,R_OK)==0;
}

static char *gui_strdup(const char *text){
    if(text==NULL){
        return NULL;
    }
    size_t size=strlen(text)+1;
    char *copy=malloc(size);
    if(copy!=NULL){
        memcpy(copy,text,size);
    }
    return copy;
}

/* 把一项追加到 ':' 分隔的搜索路径；失败返回 0，原指针保持有效 */
static int gui_path_list_add(char **list,const char *entry){
    if(entry==NULL||entry[0]=='\0'){
        return 1;
    }

    size_t old_size=(*list!=NULL)?strlen(*list):0;
    size_t entry_size=strlen(entry);
    int colon=old_size>0;

    char *grown=realloc(*list,old_size+(size_t)colon+entry_size+1);
    if(grown==NULL){
        return 0;
    }
    size_t pos=old_size;
    if(colon){
        grown[pos++]=':';
    }
    memcpy(grown+pos,entry,entry_size+1);
    *list=grown;
    return 1;
}

void sdlgui_i18n_init(void){
    setlocale(LC_ALL,"");
#ifdef SDLGUI_HAVE_GETTEXT
    /* 优先找可执行文件/应用包旁边的 locale，没有再退回编译时的 SDLGUI_LOCALEDIR */
    const char *locale_dir=SDLGUI_LOCALEDIR;
    char *from_base=NULL;
    char *from_parent=NULL;

    const char *base=SDL_GetBasePath();
    if(base!=NULL){
        from_base=gui_join_path(base,"locale");
        if(!gui_path_readable(from_base)){
            free(from_base);
            from_base=NULL;
            from_parent=gui_join_path(base,"../locale");
            if(!gui_path_readable(from_parent)){
                free(from_parent);
                from_parent=NULL;
            }
        }
    }
    if(from_base!=NULL){
        locale_dir=from_base;
    }else if(from_parent!=NULL){
        locale_dir=from_parent;
    }

    bindtextdomain(SDLGUI_TEXT_DOMAIN,locale_dir);
    bind_textdomain_codeset(SDLGUI_TEXT_DOMAIN,"UTF-8");
    textdomain(SDLGUI_TEXT_DOMAIN);

    free(from_base);
    free(from_parent);
#endif
}


static SDL_Window *window=NULL;
SDL_Renderer *renderer=NULL;

static float ui_scale=1.0f;   /* 逻辑点 -> 物理像素，启动时锁定 */
static int ui_scale_locked=0; /* 字体/纹理缓存按启动时的缩放栅格化 */
static int logical_width=0;   /* 当前逻辑呈现尺寸，0 表示还没设置 */
static int logical_height=0;

#ifdef SDLGUI_FONT_BACKEND_FONTCONFIG
static int fontconfig_ready=0;
#endif

float sdlgui_get_scale(void){
    return ui_scale;
}

/* 让逻辑坐标与窗口点一一对应，物理输出由 SDL 放大到 Retina 像素。
 * ui_scale 供字体栅格化使用，避免高分屏上文字只有一半大小。 */
void sdlgui_sync_render_geometry(void){
    if(window==NULL||renderer==NULL){
        return;
    }

    int w=0,h=0;
    int pixel_w=0,pixel_h=0;
    if(!SDL_GetWindowSize(window,&w,&h)||w<=0||h<=0){
        return;
    }

    if(SDL_GetWindowSizeInPixels(window,&pixel_w,&pixel_h)&&pixel_w>0&&pixel_h>0){
        float scale=(float)pixel_w/(float)w;
        /* 字体和文字纹理在启动时按当时的缩放栅格化；之后换显示器只调整
         * 逻辑呈现，避免纹理尺寸和坐标比例对不上（会裁掉一半文字）。 */
        if(scale>0.0f&&!ui_scale_locked){
            ui_scale=scale;
        }
    }

    if(w!=logical_width||h!=logical_height){
        if(SDL_SetRenderLogicalPresentation(renderer,w,h,SDL_LOGICAL_PRESENTATION_STRETCH)){
            logical_width=w;
            logical_height=h;
        }else{
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        _("SetRenderLogicalPresentation failed: %s"),SDL_GetError());
        }
    }
}


static float mouse_pos[]={0,0};   /* 鼠标在窗口内的位置 */
static int mouse_buttons[]={0,0,0,0,0};   /* 左 中 右 X1 X2，1=按下 0=未按下 */
static float mouse_wheel[]={0.0f,0.0f};   /* 两次 update 之间累积的滚动增量 */




void set_mouse_pos(float x,float y){
    mouse_pos[0]=x;
    mouse_pos[1]=y;
}
void set_mouse_wheel(float x,float y){
    mouse_wheel[0]+=x;
    mouse_wheel[1]+=y;
}
float get_mouse_x(void){
    return mouse_pos[0];
}
float get_mouse_y(void){
    return mouse_pos[1];
}
float get_mouse_wheel_x(void){
    return mouse_wheel[0];
}
float get_mouse_wheel_y(void){
    return mouse_wheel[1];
}

void sdlgui_consume_wheel(void){
    mouse_wheel[0]=0.0f;
    mouse_wheel[1]=0.0f;
}

void set_mouse_button(int left,int mid,int right,int x1,int x2){
    mouse_buttons[0]=left?1:0;
    mouse_buttons[1]=mid?1:0;
    mouse_buttons[2]=right?1:0;
    mouse_buttons[3]=x1?1:0;
    mouse_buttons[4]=x2?1:0;
}
int get_mouse_left(void){
    return mouse_buttons[0];
}
int get_mouse_mid(void){
    return mouse_buttons[1];
}
int get_mouse_right(void){
    return mouse_buttons[2];
}
int get_mouse_x1(void){
    return mouse_buttons[3];
}
int get_mouse_x2(void){
    return mouse_buttons[4];
}


/* 在主题搜索路径中查找 <name>/config.toml，返回 malloc 路径 */
static char *find_theme_config(const char *theme_path,const char *name){
    size_t size=strlen(name)+sizeof("/config.toml");
    char *target=malloc(size);
    if(target==NULL){
        return NULL;
    }
    snprintf(target,size,"%s/config.toml",name);
    char *found=confc_find_path(theme_path,target);
    free(target);
    return found;
}

/* 运行时主题目录：可执行文件/应用包里的 assets、当前平台的用户/系统目录。
 * SDLGUI_THEME_PATH 环境变量会完全覆盖这份列表（见 load_theme_by_name）。 */
static char *build_theme_search_path(void){
    char *list=NULL;

    const char *base=SDL_GetBasePath();
    if(base!=NULL){
        static const char *subdirs[]={"assets","../assets","themes"};
        for(size_t i=0;i<sizeof(subdirs)/sizeof(subdirs[0]);i++){
            char *entry=gui_join_path(base,subdirs[i]);
            if(entry!=NULL){
                gui_path_list_add(&list,entry);
                free(entry);
            }
        }
    }

    gui_path_list_add(&list,SDLGUI_THEME_PATH);
    return list;
}

/* 主题搜索路径：SDLGUI_THEME_PATH 完全覆盖，否则用内建列表。 */
static const char *theme_search_path(char **owned){
    *owned=NULL;
    const char *path=getenv("SDLGUI_THEME_PATH");
    if(path==NULL||path[0]=='\0'){
        *owned=build_theme_search_path();
        path=*owned!=NULL?*owned:SDLGUI_THEME_PATH;
    }
    return path;
}

/* theme.toml 里保存 theme 名到实际主题目录名的映射。 */
static char *theme_map_lookup(const char *theme_path,const char *key){
    char *file=confc_find_path(theme_path,"theme.toml");
    if(file==NULL){
        return NULL;
    }

    char *name=NULL;
    toml_t t=toml_load(file);
    if(t.size>0){
        toml_type_t *tp=toml_lookup(&t,"",0,key);
        if(tp!=NULL&&*tp==TOML_STRING){
            const char *value=((toml_string_t *)tp)->str;
            if(value!=NULL&&value[0]!='\0'){
                name=gui_strdup(value);
            }
        }
    }
    toml_unload(&t);
    free(file);
    return name;
}

/* 根据 theme.toml 把传入值换算成主题名；NULL/空串从 Default 键开始。 */
static char *resolve_theme_name(const char *theme_path,const char *theme){
    const char *key=(theme!=NULL&&theme[0]!='\0')?theme:"Default";
    char *mapped=theme_map_lookup(theme_path,key);
    return mapped!=NULL?mapped:gui_strdup(key);
}

static void load_theme_config(const char *theme_path,const char *name){
    char *found=find_theme_config(theme_path,name);
    if(found==NULL&&strcmp(name,"Default")!=0){
        found=find_theme_config(theme_path,"Default");
    }
    if(found==NULL){
        return;   /* 保持内置主题 */
    }

    if(found[0]=='/'){
        theme_set(NULL,found);
    }else{
        theme_set(".",found);
    }
    free(found);
}

/* 独立的主题加载：先查 theme.toml 映射，再按主题名加载配置并选择系统亮/暗。 */
void sdlgui_theme_init(const char *theme){
    char *owned=NULL;
    const char *theme_path=theme_search_path(&owned);

    char *name=resolve_theme_name(theme_path,theme);
    if(name!=NULL){
        load_theme_config(theme_path,name);
        free(name);
    }
    free(owned);

    /* 内置主题也跟随系统亮/暗；加载主题后 theme_set() 会再应用一次 */
    theme_select_system();
}

/* 找资源文件：先按当前目录/绝对路径找，再找可执行文件旁边和上一级。
 * 应用包里的 assets 在 Contents/Resources/assets，开发时在仓库 assets。 */
static char *resolve_asset_path(const char *path){
    if(path==NULL||path[0]=='\0'){
        return NULL;
    }
    if(gui_path_readable(path)){
        return gui_strdup(path);
    }
    if(path[0]=='/'){
        return NULL;
    }

    const char *base=SDL_GetBasePath();
    if(base==NULL){
        return NULL;
    }

    char *result=NULL;
    char *candidate=gui_join_path(base,path);
    if(gui_path_readable(candidate)){
        result=candidate;
        candidate=NULL;
    }
    free(candidate);

    if(result==NULL){
        char *parent=gui_join_path(base,"..");
        if(parent!=NULL){
            candidate=gui_join_path(parent,path);
            if(gui_path_readable(candidate)){
                result=candidate;
                candidate=NULL;
            }
            free(candidate);
            free(parent);
        }
    }

    return result;
}

void sdlgui_init(const char *title,int width,int height,const char *iconpath,const char *theme){
    /* 内建主题只使用 C locale；加载主题后再由 theme_set() 启用 gettext */
    setlocale(LC_ALL,"C");
    if(!SDL_Init(SDL_INIT_VIDEO)||!TTF_Init()){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("SDL_Init failed: %s"),SDL_GetError());
        exit(1);
    }

    if(!SDL_CreateWindowAndRenderer(title,width,height,
                                   SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                   &window,&renderer)){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Create window/renderer failed: %s"),SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        exit(1);
    }

    SDL_SetRenderVSync(renderer,1);

    /* 先确定 Retina 缩放和逻辑坐标，后面字体按这个比例栅格化 */
    sdlgui_sync_render_geometry();

#ifdef SDLGUI_FONT_BACKEND_FONTCONFIG
    if(!FcInit()){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("FcInit failed"));
        sdlgui_quit();
        exit(1);
    }
    fontconfig_ready=1;
#endif

    if(!theme_init()){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to initialize the theme"));
        sdlgui_quit();
        exit(1);
    }

    /* 独立的主题加载：theme.toml 映射 + 配置加载 + 系统亮/暗选择 */
    sdlgui_theme_init(theme);

    ui_scale_locked=1;

    if(iconpath!=NULL){
        /* macOS 上窗口图标无效，但开发时其他平台仍然需要 */
        char *resolved=resolve_asset_path(iconpath);
        SDL_Surface *icon=SDL_LoadSurface(resolved!=NULL?resolved:iconpath);
        free(resolved);
        if(!icon){
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,_("Icon load failed: %s"),SDL_GetError());
        }else{
            if(!SDL_SetWindowIcon(window,icon)){
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,_("Set icon failed: %s"),SDL_GetError());
            }
            SDL_DestroySurface(icon);
        }
    }


}
void sdlgui_quit(void){
    input_quit();
    layout_quit();
    theme_quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
#ifdef SDLGUI_FONT_BACKEND_FONTCONFIG
    if(fontconfig_ready){
        FcFini();
        fontconfig_ready=0;
    }
#endif
    TTF_Quit();
    SDL_Quit();
}


void set_window(SDL_Window *w){
    window=w;
}
SDL_Window *get_window(void){
    return window;
}
void set_renderer(SDL_Renderer *r){
    renderer=r;
}
SDL_Renderer *get_renderer(void){
    return renderer;
}



/* ---- 统一事件处理 ---- */

int sdlgui_handle_event(const SDL_Event *event){
    if(event==NULL){
        return 1;
    }

    /* 焦点输入框优先处理文本和编辑键 */
    if(layout_input_event(event)){
        return 1;
    }

    SDL_Window *w=get_window();

    switch(event->type){
    case SDL_EVENT_QUIT:
        return 0;
    case SDL_EVENT_KEY_DOWN:
        if(event->key.key==SDLK_ESCAPE){
            return 0;
        }
        break;
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        update_root();
        break;
    case SDL_EVENT_SYSTEM_THEME_CHANGED:
        theme_select_system();
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if(w!=NULL&&event->motion.windowID==SDL_GetWindowID(w)){
            set_mouse_pos(event->motion.x,event->motion.y);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if(w!=NULL&&event->button.windowID==SDL_GetWindowID(w)){
            set_mouse_pos(event->button.x,event->button.y);
            SDL_MouseButtonFlags buttons=SDL_GetMouseState(NULL,NULL);
            set_mouse_button((buttons&SDL_BUTTON_LMASK)!=0,
                (buttons&SDL_BUTTON_MMASK)!=0,
                (buttons&SDL_BUTTON_RMASK)!=0,
                (buttons&SDL_BUTTON_X1MASK)!=0,
                (buttons&SDL_BUTTON_X2MASK)!=0);
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        if(w!=NULL&&event->wheel.windowID==SDL_GetWindowID(w)){
            set_mouse_pos(event->wheel.mouse_x,event->wheel.mouse_y);
            set_mouse_wheel(event->wheel.x,event->wheel.y);
        }
        break;
    default:
        break;
    }

    return 1;
}

void sdlgui_render_frame(void){
    static int rendering=0;
    if(renderer==NULL||rendering){
        return;
    }
    rendering=1;

    /* 先同步窗口尺寸和逻辑呈现，再清屏/绘制。缩放窗口时如果先清屏、
     * 后更新逻辑尺寸，这一帧会按旧尺寸绘制，下一帧才恢复，看起来像
     * 旧画面留下的虚影。 */
    update_root();

    unsigned background=get_color(COLOR_MAIN);
    SDL_SetRenderDrawColor(renderer,
        (background>>24)&0xFF,(background>>16)&0xFF,
        (background>>8)&0xFF,background&0xFF);
    SDL_RenderClear(renderer);

    draw_root();

    SDL_RenderPresent(renderer);
    rendering=0;
}

/* live resize 时主循环要等事件处理完才渲染，窗口尺寸变化到下一帧之间
 * 合成器会把旧 buffer 拉伸到新尺寸，表现为拖影/虚影。这里在尺寸事件
 * 到达时立刻重绘，EXPOSED 仍然保留（macOS 拖动边缘时主要靠它）。 */
static bool SDLCALL sdlgui_live_resize_watch(void *userdata,SDL_Event *event){
    (void)userdata;
    switch(event->type){
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        break;
    default:
        return true;
    }

    SDL_Window *w=get_window();
    if(w!=NULL&&event->window.windowID==SDL_GetWindowID(w)){
        sdlgui_render_frame();
    }
    return true;
}

void sdlgui_enable_live_resize(void){
    SDL_AddEventWatch(sdlgui_live_resize_watch,NULL);
}

void sdlgui_disable_live_resize(void){
    SDL_RemoveEventWatch(sdlgui_live_resize_watch,NULL);
}
