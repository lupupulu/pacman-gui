#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "sdlgui.h"
#include "sdlgui_i18n.h"
#include "sdlgui_fontlookup.h"
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
static SDL_Renderer *renderer=NULL;
static layout_t root={.son_resizable=1,.enable=1,.active=1,.update=layout_update};

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
static void sync_render_geometry(void){
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

static layout_input_t *focused_input=NULL;
static layout_input_t *input_hover=NULL;
static SDL_Cursor *input_text_cursor=NULL;
static int input_last_left=0;



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
    sync_render_geometry();

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
    delete_layout(&root);
    if(input_text_cursor!=NULL){
        SDL_SetCursor(SDL_GetDefaultCursor());
        SDL_DestroyCursor(input_text_cursor);
        input_text_cursor=NULL;
    }
    focused_input=NULL;
    input_hover=NULL;
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



void resize_root(int w,int h){
    root.width=w;
    root.height=h;
}

void update_root(void){
    if(window==NULL||renderer==NULL){
        return;
    }

    /* 布局一律用窗口的逻辑尺寸（点），鼠标事件坐标也是同一套坐标 */
    sync_render_geometry();

    int w=0,h=0;
    if(!SDL_GetWindowSize(window,&w,&h)){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Get size of window failed: %s"),SDL_GetError());
        return;
    }
    if(w<=0||h<=0){
        return;   /* 最小化时保留上一次的布局尺寸 */
    }
    root.width=(float)w;
    root.height=(float)h;

    layout_mouse(&root);
    mouse_wheel[0]=0.0f;   /* 滚轮增量只在本帧的鼠标遍历中生效 */
    mouse_wheel[1]=0.0f;
    update_sons(&root);
}
const layout_t *get_root(void){
    return &root;
}
layout_t *get_root_layout(void){
    return &root;
}

float get_root_w(void){
    return root.width;
}
float get_root_h(void){
    return root.height;
}


/* 把绘制限制在 [x,x+w) x [y,y+h) 内；已有裁剪区时取交集。
 * 成功返回 1，old/had_clip 用于 draw_clip_end() 恢复原来的裁剪状态。 */
static int draw_clip_begin(float x,float y,float w,float h,SDL_Rect *old,int *had_clip){
    if(renderer==NULL){
        return 0;
    }
    if(w<0.0f){
        x+=w;
        w=-w;
    }
    if(h<0.0f){
        y+=h;
        h=-h;
    }

    SDL_Rect clip={
        (int)SDL_floorf(x),(int)SDL_floorf(y),
        (int)SDL_ceilf(x+w)-(int)SDL_floorf(x),
        (int)SDL_ceilf(y+h)-(int)SDL_floorf(y)
    };
    if(clip.w<=0||clip.h<=0){
        return 0;
    }

    *had_clip=SDL_RenderClipEnabled(renderer)?1:0;
    *old=(SDL_Rect){0,0,0,0};
    if(*had_clip){
        if(!SDL_GetRenderClipRect(renderer,old)){
            return 0;
        }
        int left=clip.x>old->x?clip.x:old->x;
        int top=clip.y>old->y?clip.y:old->y;
        int right=clip.x+clip.w<old->x+old->w?clip.x+clip.w:old->x+old->w;
        int bottom=clip.y+clip.h<old->y+old->h?clip.y+clip.h:old->y+old->h;
        if(right<=left||bottom<=top){
            return 0;
        }
        clip=(SDL_Rect){left,top,right-left,bottom-top};
    }

    return SDL_SetRenderClipRect(renderer,&clip)?1:0;
}

static void draw_clip_end(const SDL_Rect *old,int had_clip){
    SDL_SetRenderClipRect(renderer,had_clip?old:NULL);
}

/* 递归绘制布局树。每个布局先把裁剪区收窄到自身范围，再绘制自己 */
static void draw_layout(layout_t *l){
    if(l==NULL||!l->enable){
        return;
    }

    SDL_Rect old;
    int had_clip=0;
    if(!draw_clip_begin(l->x,l->y,l->width,l->height,&old,&had_clip)){
        return;
    }

    if(l->draw){
        l->draw(l);
    }
    for(size_t i=0;i<l->son_size;i++){
        draw_layout(l->sons[i]);
    }

    draw_clip_end(&old,had_clip);
}

void draw_root(void){
    draw_layout(&root);
}


void rect_draw(void *p){
    layout_rect_t *rect=p;
    layout_t *l=&rect->l;
    SDL_FRect box={.x=l->x,.y=l->y,.w=l->width,.h=l->height};
    SDL_SetRenderDrawColor(renderer,
        (Uint8)((rect->color>>24)&0xFF),
        (Uint8)((rect->color>>16)&0xFF),
        (Uint8)((rect->color>>8)&0xFF),
        (Uint8)(rect->color&0xFF));
    SDL_RenderFillRect(renderer,&box);
}

void *create_layout(size_t siz,layout_t *parent,float w,float h,draw_func draw,update_func update,clear_func clear,mouse_func mouse,align_t align){
    if(siz<sizeof(layout_t)){
        return NULL;
    }
    if(parent!=NULL&&parent->son_resizable==FALSE){
        return NULL;
    }

    layout_t *new=malloc(siz);
    if(new==NULL){
        return NULL;
    }

    memset(new,0,siz);

    if(parent){
        layout_t **sons=realloc(parent->sons,(parent->son_size+1)*sizeof(layout_t*));
        if(sons==NULL){
            free(new);
            return NULL;
        }
        parent->sons=sons;
        parent->sons[parent->son_size]=new;
        parent->son_size++;
    }

    new->enable=1;         // 新建的布局默认启用
    new->active=1;
    new->son_resizable=1;  // sons 数组是 realloc 出来的，可以继续增删、需要 free
    new->need_free=1;      // 本结构体是 malloc 出来的，delete_layout 时要 free
    new->align=align;
    new->parent=(sublayout_t*)parent;   /* layout_t 与 sublayout_t 前缀相同，可互转 */
    new->width=w;
    new->height=h;
    new->draw=draw;
    new->update=update;
    new->clear=clear;
    new->mouse=mouse;

    return new;
}

/* 从旧 parent 的 sons 中摘下，不释放布局本身 */
void layout_detach(layout_t *child){
    if(child==NULL||child->parent==NULL){
        return;
    }
    layout_t *parent=(layout_t *)child->parent;
    for(size_t i=0;i<parent->son_size;i++){
        if(parent->sons[i]==child){
            memmove(&parent->sons[i],&parent->sons[i+1],
                    (parent->son_size-i-1)*sizeof(*parent->sons));
            parent->son_size--;
            break;
        }
    }
    child->parent=NULL;
}

/* 挂接到 parent；如果原来有 parent，会先从旧 parent 摘下 */
int layout_attach(layout_t *child,layout_t *parent){
    if(child==NULL||parent==NULL||parent->son_resizable==FALSE){
        return 0;
    }
    if(child->parent==(sublayout_t *)parent){
        return 1;
    }

    layout_t **sons=realloc(parent->sons,(parent->son_size+1)*sizeof(*parent->sons));
    if(sons==NULL){
        return 0;
    }
    parent->sons=sons;
    layout_detach(child);
    parent->sons[parent->son_size++]=child;
    child->parent=(sublayout_t *)parent;
    return 1;
}

void delete_layout(layout_t *l){
    for(size_t i=0;i<l->son_size;i++){
        delete_layout(l->sons[i]);
    }
    if(l->son_resizable){
        free(l->sons);
        l->sons=NULL;
    }
    if(l->clear){
        l->clear(l);
    }
    if(l->need_free){
        free(l);
    }
}

void update_sons(void *p){
    layout_t *l=p;

    layout_t **son=l->sons;
    for(size_t i=0;i<l->son_size;i++){
        if(!son[i]->enable){
            continue;
        }
        if(son[i]->update){
            son[i]->update(son[i]);
        }
        if(l->x<get_root_w()*2&&l->y<get_root_h()*2&&l->x+l->width>-get_root_w()*2&&l->y+l->height>-get_root_h()*2){
            update_sons(son[i]);
        }
    }
}

static int layout_contains_mouse(const layout_t *l){
    if(l==NULL||l->width<0.0f||l->height<0.0f){
        return 0;
    }
    float mx=get_mouse_x();
    float my=get_mouse_y();
    return mx>=l->x&&mx<=l->x+l->width&&my>=l->y&&my<=l->y+l->height;
}

/* 派发一个子布局的鼠标更新：
 * - 鼠标在子布局内：正常递归/调用 mouse
 * - 刚离开：额外调用一次，让 hover/按下状态能清掉
 * - 离开时左键仍按住：继续跟踪，直到松开；避免拖拽状态卡住
 * 其余情况不发送更新。 */
static void layout_mouse_dispatch(layout_t *son){
    if(son==NULL||!son->enable||!son->active){
        return;
    }

    int was_inside=son->mouse_inside;
    int inside=layout_contains_mouse(son);
    son->mouse_inside=inside;

    int tracking=son->mouse_tracking;
    int button_down=mouse_buttons[0]!=0;
    if(!inside&&!was_inside&&!tracking){
        return;
    }

    layout_mouse(son);
    if(son->mouse){
        son->mouse(son);
    }

    if(inside){
        son->mouse_tracking=0;
    }else if(was_inside&&button_down){
        son->mouse_tracking=1;
    }else if(!button_down){
        son->mouse_tracking=0;
    }
}

void layout_mouse(void *p){
    layout_t *l=p;
    if(l==NULL){
        return;
    }

    if(l==&root){
        int left=mouse_buttons[0]!=0;
        if(left&&!input_last_left&&focused_input!=NULL&&
           !layout_contains_mouse(&focused_input->l)){
            layout_input_blur();
        }
        input_last_left=left;
    }

    for(size_t i=0;i<l->son_size;i++){
        layout_mouse_dispatch(l->sons[i]);
    }
}



void layout_update(void *p){
    sublayout_t *l=p;
    if(l==NULL){
        return;
    }
    sublayout_t *parent=l->parent;
    if(parent==NULL){
        return;
    }

    switch(l->align.width){
    case 1:
        l->width = parent->width * l->align.width_offset;
        break;
    case 2:
        l->width = parent->width + l->align.width_offset;
        break;
    }

    switch(l->align.horizon){
    case 1:
        l->x = parent->x + parent->width;
        break;
    case 2:
        l->x = parent->x - l->width;
        break;
    case 3:
        l->x = parent-> x + parent->width/2 - l->width/2;
        break;
    }

    if(l->align.horizon){
        l->x+=l->align.horizon_offset;
    }



    switch(l->align.height){
    case 1:
        l->height = parent->height * l->align.height_offset;
        break;
    case 2:
        l->height = parent->height + l->align.height_offset;
        break;
    }

    switch(l->align.vertical){
    case 1:
        l->y = parent->y + parent->height;
        break;
    case 2:
        l->y = parent->y - l->height;
        break;
    case 3:
        l->y = parent->y + parent->height/2 - l->height/2;
        break;
    }

    if(l->align.vertical){
        l->y+=l->align.vertical_offset;
    }
}



layout_rect_t *create_layout_rect(void *parent,float w,float h,align_t align,unsigned color){
    layout_t *p=parent?parent:&root;
    layout_rect_t *r=create_layout(sizeof(layout_rect_t),p,w,h,rect_draw,layout_update,NULL,NULL,align);
    r->color=color;
    return r;
}


static void label_position_text(layout_label_t *label){
    if(label==NULL||label->text==NULL){
        return;
    }

    draft_text_t *text=label->text;
    float x=label->l.x;
    float y=label->l.y;

    if(label->text_align.horizon==1){
        x=label->l.x+label->l.width-text->width;
    }else if(label->text_align.horizon==3){
        x=label->l.x+(label->l.width-text->width)/2.0f;
    }
    if(label->text_align.vertical==1){
        y=label->l.y+label->l.height-text->height;
    }else if(label->text_align.vertical==3){
        y=label->l.y+(label->l.height-text->height)/2.0f;
    }

    text->x=x+label->text_align.horizon_offset;
    text->y=y+label->text_align.vertical_offset;
}

/* label 的尺寸变化时，文字要按新的宽度重新换行、按新的高度裁剪。
 * 清掉旧缓存，下一次 draft_draw_text() 会用新限制重建。 */
static void label_sync_text_limits(layout_label_t *label){
    if(label==NULL||label->text==NULL){
        return;
    }
    if(label->text->width_limit!=label->l.width||
       label->text->height_limit!=label->l.height){
        label->text->width_limit=label->l.width;
        label->text->height_limit=label->l.height;
        draft_text_clear(label->text);
    }
}

static void label_update(void *p){
    layout_label_t *label=p;
    if(label==NULL){
        return;
    }
    layout_update(&label->l);
    label_sync_text_limits(label);
    label_position_text(label);
}

void label_draw(void *p){
    layout_label_t *label=p;
    if(label==NULL||label->text==NULL||renderer==NULL){
        return;
    }

    layout_update(&label->l);
    label_sync_text_limits(label);
    label_position_text(label);
    draft_draw_text(label->text);
}

static void label_clear(void *p){
    layout_label_t *label=p;
    if(label==NULL){
        return;
    }
    if(label->text!=NULL){
        draft_text_clear(label->text);
        free(label->text);
        label->text=NULL;
    }
    free(label->context);
    label->context=NULL;
}

layout_label_t *create_layout_label(void *parent,float w,float h,align_t align,
    align_t text_align,TTF_Font *font,const char *context,unsigned color){
    if(font==NULL||w<=0.0f||h<=0.0f){
        return NULL;
    }

    const char *text=context!=NULL?context:"";
    size_t length=strlen(text);
    char *copy=malloc(length+1);
    if(copy==NULL){
        return NULL;
    }
    memcpy(copy,text,length+1);

    layout_t *p=parent?parent:&root;
    layout_label_t *label=create_layout(
        sizeof(*label),
        p,
        w,
        h,
        label_draw,
        label_update,
        label_clear,
        NULL,
        align
    );
    if(label==NULL){
        free(copy);
        return NULL;
    }

    label->context=copy;
    label->text_align=text_align;

    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    label->text=draft_create_text(zero,font,label->context,w,h,color);
    if(label->text==NULL){
        delete_layout((layout_t *)label);
        return NULL;
    }
    label->text->parent=(sublayout_t *)&label->l;

    label_update(label);
    return label;
}



/* ---- 绘制草稿（draft） ----
   draft 不是 layout，不进入 layout 的 sons 树；它由 create 分配，由
   layout_draw_t 之类的容器按 draw 函数指针调用。parent 留给容器挂接时填写，
   未挂接时按根布局定位；draw 里会用 align 重新算一次 x/y/宽高。 */

#define DRAFT_SUBLAYOUT(d) ((sublayout_t *)&(d)->parent)

static int draft_text_build_cache(draft_text_t *d);

static void draft_update_layout(sublayout_t *sl){
    if(sl->parent==NULL){
        sl->parent=(sublayout_t *)&root;
    }
    layout_update(sl);
}

static SDL_Color draft_color(unsigned color){
    SDL_Color c={
        .r=(Uint8)((color>>24)&0xFF),
        .g=(Uint8)((color>>16)&0xFF),
        .b=(Uint8)((color>>8)&0xFF),
        .a=(Uint8)(color&0xFF)
    };
    return c;
}

static SDL_FColor draft_fcolor(unsigned color){
    SDL_Color c=draft_color(color);
    SDL_FColor f={
        .r=c.r/255.0f,
        .g=c.g/255.0f,
        .b=c.b/255.0f,
        .a=c.a/255.0f
    };
    return f;
}

static int draft_has_border(unsigned color){
    return (color&0xFF)!=0;
}

static void draft_set_draw_color(unsigned color){
    SDL_Color c=draft_color(color);
    SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);
}

#define DRAFT_CIRCLE_MAX_SEGMENTS 96

/* 用三角扇画实心圆：比逐行 sqrt 更贴合圆形轮廓，圆角处不会出现台阶 */
static void draft_fill_circle_geometry(float cx,float cy,float radius,SDL_Color color){
    if(radius<=0.0f){
        return;
    }

    int segments=(int)(radius*3.0f);
    if(segments<16){
        segments=16;
    }else if(segments>DRAFT_CIRCLE_MAX_SEGMENTS){
        segments=DRAFT_CIRCLE_MAX_SEGMENTS;
    }

    SDL_Vertex vertices[DRAFT_CIRCLE_MAX_SEGMENTS+1];
    int indices[DRAFT_CIRCLE_MAX_SEGMENTS*3];
    SDL_FColor fcolor={
        .r=(float)color.r/255.0f,
        .g=(float)color.g/255.0f,
        .b=(float)color.b/255.0f,
        .a=(float)color.a/255.0f
    };

    vertices[0]=(SDL_Vertex){
        .position={cx,cy},
        .color=fcolor,
        .tex_coord={0.0f,0.0f}
    };
    for(int i=0;i<segments;i++){
        float angle=(float)i*2.0f*SDL_PI_F/(float)segments;
        vertices[i+1]=(SDL_Vertex){
            .position={cx+SDL_cosf(angle)*radius,cy+SDL_sinf(angle)*radius},
            .color=fcolor,
            .tex_coord={0.0f,0.0f}
        };

        indices[i*3]=0;
        indices[i*3+1]=i+1;
        indices[i*3+2]=(i+1==segments)?1:i+2;
    }

    SDL_RenderGeometry(renderer,NULL,vertices,segments+1,indices,segments*3);
}

/* 实心圆 */
static void draft_fill_circle(float cx,float cy,float radius,SDL_Color color){
    draft_fill_circle_geometry(cx,cy,radius,color);
}

/* 胶囊形：中间矩形 + 两端圆形。用于全圆角（包括负半径）的情况。 */
static void draft_fill_capsule(float x,float y,float w,float h,SDL_Color color){
    if(w<=0.0f||h<=0.0f){
        return;
    }

    SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,color.a);
    if(w>h){
        float r=h/2.0f;
        SDL_FRect middle={.x=x+r,.y=y,.w=w-r*2.0f,.h=h};
        if(middle.w>0.0f){
            SDL_RenderFillRect(renderer,&middle);
        }
        draft_fill_circle_geometry(x+r,y+r,r,color);
        draft_fill_circle_geometry(x+w-r,y+r,r,color);
    }else if(w<h){
        float r=w/2.0f;
        SDL_FRect middle={.x=x,.y=y+r,.w=w,.h=h-r*2.0f};
        if(middle.h>0.0f){
            SDL_RenderFillRect(renderer,&middle);
        }
        draft_fill_circle_geometry(x+r,y+r,r,color);
        draft_fill_circle_geometry(x+r,y+h-r,r,color);
    }else{
        draft_fill_circle_geometry(x+w/2.0f,y+h/2.0f,w/2.0f,color);
    }
}

#define DRAFT_RRECT_MAX_SEGMENTS 64

/* 实心圆角矩形：用中心三角扇覆盖圆角外轮廓。
 * 不再逐行 sqrt 扫描，避免圆弧处出现横向台阶。 */
static void draft_fill_rrect(float x,float y,float w,float h,float r,SDL_Color color){
    if(w<=0.0f||h<=0.0f){
        return;
    }
    float max_r=(w<h?w:h)/2.0f;
    if(r>max_r){ r=max_r; }
    if(r<0.0f){ r=0.0f; }

    if(r<=0.0f){
        SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,color.a);
        SDL_FRect rect={.x=x,.y=y,.w=w,.h=h};
        SDL_RenderFillRect(renderer,&rect);
        return;
    }

    int segments=(int)(r*1.5f);
    if(segments<6){
        segments=6;
    }else if(segments>DRAFT_RRECT_MAX_SEGMENTS){
        segments=DRAFT_RRECT_MAX_SEGMENTS;
    }

    enum{ MAX_POINTS=4*(DRAFT_RRECT_MAX_SEGMENTS+1) };
    SDL_Vertex vertices[MAX_POINTS+1];
    int indices[MAX_POINTS*3];
    SDL_FColor fcolor={
        .r=(float)color.r/255.0f,
        .g=(float)color.g/255.0f,
        .b=(float)color.b/255.0f,
        .a=(float)color.a/255.0f
    };

    int count=0;
    vertices[count++]=(SDL_Vertex){
        .position={x+w/2.0f,y+h/2.0f},
        .color=fcolor,
        .tex_coord={0.0f,0.0f}
    };

    const float centers[4][2]={
        {x+r,y+r},
        {x+w-r,y+r},
        {x+w-r,y+h-r},
        {x+r,y+h-r}
    };
    const float starts[4]={
        SDL_PI_F,
        SDL_PI_F*1.5f,
        0.0f,
        SDL_PI_F*0.5f
    };

    for(int corner=0;corner<4;corner++){
        for(int i=0;i<=segments;i++){
            float angle=starts[corner]+SDL_PI_F*0.5f*(float)i/(float)segments;
            vertices[count++]=(SDL_Vertex){
                .position={
                    centers[corner][0]+SDL_cosf(angle)*r,
                    centers[corner][1]+SDL_sinf(angle)*r
                },
                .color=fcolor,
                .tex_coord={0.0f,0.0f}
            };
        }
    }

    int perimeter=count-1;
    for(int i=0;i<perimeter;i++){
        indices[i*3]=0;
        indices[i*3+1]=i+1;
        indices[i*3+2]=(i+1==perimeter)?1:i+2;
    }

    SDL_RenderGeometry(renderer,NULL,vertices,count,indices,perimeter*3);
}

draft_default_t *draft_create_default(draw_func draw,align_t align,float w,float h,unsigned color,unsigned border_color){
    draft_default_t *d=calloc(1,sizeof(*d));
    if(d==NULL){
        return NULL;
    }
    d->draw=draw?draw:draft_draw_rect;
    d->parent=NULL;
    d->align=align;
    d->width=w;
    d->height=h;
    d->color=color;
    d->border_color=border_color;
    return d;
}

void draft_draw_rect(void *p){
    draft_default_t *d=p;
    if(d==NULL||renderer==NULL){
        return;
    }
    draft_update_layout(DRAFT_SUBLAYOUT(d));
    if(d->width<=0.0f||d->height<=0.0f){
        return;
    }

    SDL_FRect box={.x=d->x,.y=d->y,.w=d->width,.h=d->height};
    if(draft_has_border(d->border_color)){
        draft_set_draw_color(d->border_color);
        SDL_RenderFillRect(renderer,&box);
        if(d->width>2.0f&&d->height>2.0f){
            SDL_FRect inner={
                .x=d->x+1.0f,.y=d->y+1.0f,
                .w=d->width-2.0f,.h=d->height-2.0f
            };
            draft_set_draw_color(d->color);
            SDL_RenderFillRect(renderer,&inner);
        }
    }else{
        draft_set_draw_color(d->color);
        SDL_RenderFillRect(renderer,&box);
    }
}

void draft_draw_circle_w(void *p){
    draft_default_t *d=p;
    if(d==NULL||renderer==NULL){
        return;
    }
    draft_update_layout(DRAFT_SUBLAYOUT(d));
    float radius=d->width/2.0f;
    float cx=d->x+d->width/2.0f;
    float cy=d->y+d->height/2.0f;

    if(draft_has_border(d->border_color)){
        draft_fill_circle(cx,cy,radius,draft_color(d->border_color));
        if(radius>1.0f){
            draft_fill_circle(cx,cy,radius-1.0f,draft_color(d->color));
        }
    }else{
        draft_fill_circle(cx,cy,radius,draft_color(d->color));
    }
}

void draft_draw_circle_h(void *p){
    draft_default_t *d=p;
    if(d==NULL||renderer==NULL){
        return;
    }
    draft_update_layout(DRAFT_SUBLAYOUT(d));
    float radius=d->height/2.0f;
    float cx=d->x+d->width/2.0f;
    float cy=d->y+d->height/2.0f;

    if(draft_has_border(d->border_color)){
        draft_fill_circle(cx,cy,radius,draft_color(d->border_color));
        if(radius>1.0f){
            draft_fill_circle(cx,cy,radius-1.0f,draft_color(d->color));
        }
    }else{
        draft_fill_circle(cx,cy,radius,draft_color(d->color));
    }
}

draft_triangle_t *draft_create_triangle(align_t align,float *vertex,unsigned color,unsigned border_color){
    if(vertex==NULL){
        return NULL;
    }
    draft_triangle_t *d=calloc(1,sizeof(*d));
    if(d==NULL){
        return NULL;
    }
    d->draw=draft_draw_triangle;
    d->parent=NULL;
    d->align=align;

    float min_x=vertex[0], min_y=vertex[1];
    float max_x=min_x, max_y=min_y;
    for(size_t i=1;i<3;i++){
        float x=vertex[i*2], y=vertex[i*2+1];
        if(x<min_x){ min_x=x; }
        if(x>max_x){ max_x=x; }
        if(y<min_y){ min_y=y; }
        if(y>max_y){ max_y=y; }
    }

    d->x=min_x;
    d->y=min_y;
    d->width=max_x-min_x;
    d->height=max_y-min_y;
    for(size_t i=0;i<3;i++){
        d->points[i][0]=vertex[i*2]-min_x;
        d->points[i][1]=vertex[i*2+1]-min_y;
    }
    d->color=color;
    d->border_color=border_color;
    return d;
}

void draft_draw_triangle(void *p){
    draft_triangle_t *d=p;
    if(d==NULL||renderer==NULL){
        return;
    }
    draft_update_layout(DRAFT_SUBLAYOUT(d));

    SDL_Vertex v[3];
    SDL_FColor color=draft_fcolor(d->color);
    for(size_t i=0;i<3;i++){
        v[i].position=(SDL_FPoint){
            .x=d->x+d->points[i][0],
            .y=d->y+d->points[i][1]
        };
        v[i].color=color;
        v[i].tex_coord=(SDL_FPoint){.x=0,.y=0};
    }
    SDL_RenderGeometry(renderer,NULL,v,3,NULL,0);

    if(draft_has_border(d->border_color)){
        SDL_FPoint points[4];
        for(size_t i=0;i<3;i++){
            points[i]=v[i].position;
        }
        points[3]=v[0].position;
        draft_set_draw_color(d->border_color);
        SDL_RenderLines(renderer,points,4);
    }
}

draft_rrect_t *draft_create_rrect(align_t align,float w,float h,float r,unsigned color,unsigned border_color){
    draft_rrect_t *d=calloc(1,sizeof(*d));
    if(d==NULL){
        return NULL;
    }
    int auto_radius=0;
    if(r<0.0f){
        auto_radius=1;
        r=(w<h?w:h)/2.0f;
        if(r<0.0f){
            r=0.0f;
        }
    }
    d->draw=draft_draw_rrect;
    d->parent=NULL;
    d->align=align;
    d->width=w;
    d->height=h;
    d->r=r;
    d->auto_radius=auto_radius?1u:0u;
    d->color=color;
    d->border_color=border_color;
    return d;
}

void draft_draw_rrect(void *p){
    draft_rrect_t *d=p;
    if(d==NULL||renderer==NULL){
        return;
    }
    draft_update_layout(DRAFT_SUBLAYOUT(d));
    if(d->width<=0.0f||d->height<=0.0f){
        return;
    }

    /* 负半径（auto_radius）表示“全圆角”：按当前短边的一半计算 */
    float r=d->r;
    float max_r=(d->width<d->height?d->width:d->height)/2.0f;
    if(d->auto_radius||r<0.0f){
        r=max_r;
    }else if(r>max_r){
        r=max_r;
    }

    /* 全圆角用“矩形 + 两个圆”绘制，避免逐行扫描在圆弧处产生台阶。
     * 负半径和 auto_radius 都按全圆角处理。 */
    int capsule=d->auto_radius||(r>=max_r-0.01f);
    if(capsule){
        if(draft_has_border(d->border_color)){
            draft_fill_capsule(d->x,d->y,d->width,d->height,draft_color(d->border_color));
            if(d->width>2.0f&&d->height>2.0f){
                draft_fill_capsule(d->x+1.0f,d->y+1.0f,d->width-2.0f,d->height-2.0f,draft_color(d->color));
            }
        }else{
            draft_fill_capsule(d->x,d->y,d->width,d->height,draft_color(d->color));
        }
        return;
    }

    if(draft_has_border(d->border_color)){
        draft_fill_rrect(d->x,d->y,d->width,d->height,r,draft_color(d->border_color));
        if(d->width>2.0f&&d->height>2.0f){
            float inner_r=(r>1.0f)?r-1.0f:0.0f;
            draft_fill_rrect(d->x+1.0f,d->y+1.0f,d->width-2.0f,d->height-2.0f,inner_r,draft_color(d->color));
        }
    }else{
        draft_fill_rrect(d->x,d->y,d->width,d->height,r,draft_color(d->color));
    }
}

draft_text_t *draft_create_text(align_t align,TTF_Font *font,char *context,float wl,float hl,unsigned color){
    draft_text_t *d=calloc(1,sizeof(*d));
    if(d==NULL){
        return NULL;
    }
    d->draw=draft_draw_text;
    d->clear=draft_text_clear;
    d->parent=NULL;
    d->align=align;
    d->width=0.0f;
    d->height=0.0f;
    d->can_enter=1;
    d->can_change=0;
    d->font=font;
    d->context=context;
    d->width_limit=wl;
    d->height_limit=hl;
    d->color=color;

    /* 不可变文字在 create 时直接测量并建立缓存；失败时绘制阶段会重试 */
    if(!d->can_change&&d->font!=NULL&&d->context!=NULL&&d->context[0]!='\0'&&renderer!=NULL){
        draft_text_build_cache(d);
    }
    return d;
}

/* 释放 can_change=0 时缓存的整段纹理 */
void draft_text_clear(void *p){
    draft_text_t *d=p;
    if(d==NULL){
        return;
    }
    if(d->cache!=NULL){
        SDL_DestroyTexture(d->cache);
        d->cache=NULL;
    }
    d->cache_width=0.0f;
    d->cache_height=0.0f;
}

/* can_enter=0：把换行压成空格，整段按单行处理 */
static char *draft_text_single_line(const char *context){
    size_t length=strlen(context);
    char *copy=malloc(length+1);
    if(copy==NULL){
        return NULL;
    }
    for(size_t i=0;i<length;i++){
        char c=context[i];
        copy[i]=(c=='\n'||c=='\r')?' ':c;
    }
    copy[length]='\0';
    return copy;
}

static SDL_Surface *draft_text_render_segment(draft_text_t *d,const char *text,size_t length,SDL_Color color){
    if(length==0){
        return NULL;
    }
    if(d->width_limit>0.0f){
        /* 宽度限制是逻辑坐标，换行宽度要换算成物理像素 */
        int wrap=(int)(d->width_limit*sdlgui_get_scale()+0.5f);
        if(wrap<1){
            wrap=1;
        }
        return TTF_RenderText_Blended_Wrapped(d->font,text,length,color,wrap);
    }
    return TTF_RenderText_Blended(d->font,text,length,color);
}

/* 计算最终渲染宽高（逻辑坐标）；height_limit>0 时高度按裁剪后的结果算 */
static int draft_text_measure_size(draft_text_t *d,float *out_width,float *out_height){
    if(d->font==NULL||d->context==NULL||d->context[0]=='\0'){
        return 0;
    }
    float scale=sdlgui_get_scale();
    int line_height=TTF_GetFontHeight(d->font);
    if(line_height<=0){
        return 0;
    }

    SDL_Color color=draft_color(d->color);
    const char *context=d->context;
    char *single_line=NULL;
    if(!d->can_enter){
        single_line=draft_text_single_line(context);
        if(single_line==NULL){
            return 0;
        }
        context=single_line;
    }

    int max_width=0;
    int total_height=0;
    const char *start=context;
    while(start!=NULL&&*start!='\0'){
        const char *end=d->can_enter?strchr(start,'\n'):NULL;
        size_t length=end?(size_t)(end-start):strlen(start);

        SDL_Surface *surface=draft_text_render_segment(d,start,length,color);
        if(surface!=NULL){
            if(surface->w>max_width){
                max_width=surface->w;
            }
            total_height+=(d->width_limit>0.0f)?surface->h:line_height;
            SDL_DestroySurface(surface);
        }else{
            total_height+=line_height;
        }

        if(end==NULL||end[1]=='\0'){
            break;
        }
        start=end+1;
    }
    free(single_line);

    if(max_width<=0||total_height<=0){
        return 0;
    }

    float final_height=(float)total_height/scale;
    if(d->height_limit>0.0f&&final_height>d->height_limit){
        final_height=d->height_limit;
    }
    if(final_height<=0.0f){
        return 0;
    }

    *out_width=(float)max_width/scale;
    *out_height=final_height;
    return 1;
}

/* can_change=0：把整段文本合成为一张纹理，之后只做 blit。
 * 合成表面用物理像素，d->width/height 存逻辑尺寸。 */
static int draft_text_build_cache(draft_text_t *d){
    int line_height=TTF_GetFontHeight(d->font);
    if(line_height<=0){
        return 0;
    }

    SDL_Color color=draft_color(d->color);
    const char *context=d->context;
    char *single_line=NULL;
    if(!d->can_enter){
        single_line=draft_text_single_line(context);
        if(single_line==NULL){
            return 0;
        }
        context=single_line;
    }

    /* 第一遍：算总宽高（物理像素） */
    int max_width=0;
    int total_height=0;
    const char *start=context;
    while(start!=NULL&&*start!='\0'){
        const char *end=d->can_enter?strchr(start,'\n'):NULL;
        size_t length=end?(size_t)(end-start):strlen(start);

        SDL_Surface *surface=draft_text_render_segment(d,start,length,color);
        if(surface!=NULL){
            if(surface->w>max_width){
                max_width=surface->w;
            }
            total_height+=(d->width_limit>0.0f)?surface->h:line_height;
            SDL_DestroySurface(surface);
        }else{
            total_height+=line_height;
        }

        if(end==NULL||end[1]=='\0'){
            break;
        }
        start=end+1;
    }

    if(max_width<=0||total_height<=0){
        free(single_line);
        return 0;
    }

    SDL_Surface *composite=SDL_CreateSurface(max_width,total_height,SDL_PIXELFORMAT_RGBA32);
    if(composite==NULL){
        free(single_line);
        return 0;
    }
    SDL_ClearSurface(composite,0.0f,0.0f,0.0f,0.0f);

    /* 第二遍：把每段文字画到合成表面上 */
    int y=0;
    start=context;
    while(start!=NULL&&*start!='\0'){
        const char *end=d->can_enter?strchr(start,'\n'):NULL;
        size_t length=end?(size_t)(end-start):strlen(start);

        SDL_Surface *surface=draft_text_render_segment(d,start,length,color);
        if(surface!=NULL){
            SDL_Rect dst={.x=0,.y=y,.w=surface->w,.h=surface->h};
            /* TTF 表面是直通 alpha；用 NONE 复制才能保持直通 alpha，
             * 否则 BLEND 到透明合成表面会把颜色预乘，绘制时产生深色描边。 */
            SDL_SetSurfaceBlendMode(surface,SDL_BLENDMODE_NONE);
            SDL_BlitSurface(surface,NULL,composite,&dst);
            y+=(d->width_limit>0.0f)?surface->h:line_height;
            SDL_DestroySurface(surface);
        }else{
            y+=line_height;
        }

        if(end==NULL||end[1]=='\0'){
            break;
        }
        start=end+1;
    }

    SDL_Texture *cache=SDL_CreateTextureFromSurface(renderer,composite);
    SDL_DestroySurface(composite);
    free(single_line);
    if(cache==NULL){
        return 0;
    }
    SDL_SetTextureScaleMode(cache,SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(cache,SDL_BLENDMODE_BLEND);

    float scale=sdlgui_get_scale();
    d->cache=cache;
    d->cache_width=(float)max_width;     /* 物理像素 */
    d->cache_height=(float)total_height;

    d->width=(float)max_width/scale;     /* 逻辑坐标 */
    d->height=(float)total_height/scale;
    if(d->height_limit>0.0f&&d->height>d->height_limit){
        d->height=d->height_limit;
    }
    return 1;
}

/* can_change!=0：每帧重新画，不保留缓存 */
static void draft_text_draw_direct(draft_text_t *d){
    float scale=sdlgui_get_scale();
    int line_height_px=TTF_GetFontHeight(d->font);
    if(line_height_px<=0){
        return;
    }
    float line_height=(float)line_height_px/scale;

    SDL_Color color=draft_color(d->color);
    const char *context=d->context;
    char *single_line=NULL;
    if(!d->can_enter){
        single_line=draft_text_single_line(context);
        if(single_line==NULL){
            return;
        }
        context=single_line;
    }

    float y=d->y;
    float used=0.0f;
    const char *start=context;
    while(start!=NULL&&*start!='\0'){
        const char *end=d->can_enter?strchr(start,'\n'):NULL;
        size_t length=end?(size_t)(end-start):strlen(start);

        SDL_Surface *surface=draft_text_render_segment(d,start,length,color);
        float advance=line_height;
        if(surface!=NULL){
            float draw_height=(float)surface->h/scale;
            if(d->height_limit>0.0f){
                float remain=d->height_limit-used;
                if(remain<=0.0f){
                    SDL_DestroySurface(surface);
                    break;
                }
                if(draw_height>remain){
                    draw_height=remain;
                }
            }

            SDL_Texture *tex=SDL_CreateTextureFromSurface(renderer,surface);
            if(tex!=NULL){
                SDL_SetTextureScaleMode(tex,SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_BLEND);
                float src_height=draw_height*scale;
                if(src_height>(float)surface->h){
                    src_height=(float)surface->h;
                }
                SDL_FRect src={.x=0,.y=0,.w=(float)surface->w,.h=src_height};
                SDL_FRect dst={.x=SDL_roundf(d->x),.y=SDL_roundf(y),
                               .w=(float)surface->w/scale,.h=draw_height};
                SDL_RenderTexture(renderer,tex,&src,&dst);
                SDL_DestroyTexture(tex);
            }
            if(d->width_limit>0.0f){
                advance=(float)surface->h/scale;
            }
            SDL_DestroySurface(surface);
        }

        used+=advance;
        y+=advance;
        if(d->height_limit>0.0f&&used>=d->height_limit){
            break;
        }
        if(end==NULL||end[1]=='\0'){
            break;
        }
        start=end+1;
    }

    free(single_line);
}

void draft_draw_text(void *p){
    draft_text_t *d=p;
    if(d==NULL||renderer==NULL){
        return;
    }
    if(d->font==NULL||d->context==NULL||d->context[0]=='\0'){
        draft_text_clear(d);
        return;
    }

    if(d->can_change){
        if(d->cache!=NULL){
            draft_text_clear(d);
        }
        float width=0.0f;
        float height=0.0f;
        if(!draft_text_measure_size(d,&width,&height)){
            return;
        }
        d->width=width;
        d->height=height;
        draft_update_layout(DRAFT_SUBLAYOUT(d));
        draft_text_draw_direct(d);
        return;
    }

    if(d->cache==NULL&&!draft_text_build_cache(d)){
        return;
    }

    draft_update_layout(DRAFT_SUBLAYOUT(d));

    float width=d->width;
    float height=d->height;
    if(width<=0.0f||height<=0.0f){
        return;
    }

    /* 纹理按物理像素缓存，dst 用逻辑尺寸；两者相差 ui_scale 倍 */
    float src_height=height*sdlgui_get_scale();
    if(src_height>d->cache_height){
        src_height=d->cache_height;
    }
    SDL_FRect src={.x=0,.y=0,.w=d->cache_width,.h=src_height};
    SDL_FRect dst={.x=SDL_roundf(d->x),.y=SDL_roundf(d->y),.w=width,.h=height};
    SDL_RenderTexture(renderer,d->cache,&src,&dst);
}

void draft_texture_clear(void *p){
    draft_texture_t *d=p;
    if(d==NULL){
        return;
    }
    if(d->tex!=NULL){
        if(d->owns_texture){
            SDL_DestroyTexture(d->tex);
        }
        d->tex=NULL;
    }
    d->owns_texture=0;
}

/* 解析纹理目标尺寸：w/h 都 <=0 时用原尺寸；只有一个 <=0 时按原宽高比缩放。 */
static int draft_texture_size(SDL_Texture *tex,float w,float h,float *out_w,float *out_h){
    float tex_w=0.0f;
    float tex_h=0.0f;
    if(!SDL_GetTextureSize(tex,&tex_w,&tex_h)||tex_w<=0.0f||tex_h<=0.0f){
        return 0;
    }

    if(w<=0.0f&&h<=0.0f){
        w=tex_w;
        h=tex_h;
    }else if(w<=0.0f){
        w=tex_w*(h/tex_h);
    }else if(h<=0.0f){
        h=tex_h*(w/tex_w);
    }

    *out_w=w;
    *out_h=h;
    return 1;
}

/* 公共入口只接收用户已有的纹理，不接管所有权；失败时 tex 仍归调用方。
 * w/h 任一 <=0 时按原图比例缩放；都为 <=0 时使用原尺寸。 */
draft_texture_t *draft_create_texture(align_t align,SDL_Texture *tex,float w,float h){
    if(tex==NULL){
        return NULL;
    }

    draft_texture_t *d=calloc(1,sizeof(*d));
    if(d==NULL){
        return NULL;
    }
    d->draw=draft_draw_texture;
    d->clear=draft_texture_clear;
    d->parent=NULL;
    d->align=align;
    d->tex=tex;
    d->owns_texture=0;

    if(!draft_texture_size(tex,w,h,&d->width,&d->height)){
        d->tex=NULL;
        free(d);
        return NULL;
    }
    return d;
}

/* 图片加载路径自己创建纹理，也由自己释放，所以不经过 draft_create_texture。 */
static draft_texture_t *draft_create_texture_from_surface(align_t align,SDL_Surface *surface,float w,float h){
    if(surface==NULL){
        return NULL;
    }
    if(renderer==NULL){
        SDL_DestroySurface(surface);
        return NULL;
    }

    SDL_Texture *tex=SDL_CreateTextureFromSurface(renderer,surface);
    SDL_DestroySurface(surface);
    if(tex==NULL){
        return NULL;
    }

    draft_texture_t *d=calloc(1,sizeof(*d));
    if(d==NULL){
        SDL_DestroyTexture(tex);
        return NULL;
    }
    d->draw=draft_draw_texture;
    d->clear=draft_texture_clear;
    d->parent=NULL;
    d->align=align;
    d->tex=tex;
    d->owns_texture=1;   /* 图片路径创建的纹理由 draft 释放 */

    if(!draft_texture_size(tex,w,h,&d->width,&d->height)){
        SDL_DestroyTexture(tex);
        free(d);
        return NULL;
    }
    return d;
}

draft_texture_t *draft_create_picture_from_memory(align_t align,const char *file,size_t size,float w,float h){
    if(file==NULL||size==0){
        return NULL;
    }
    SDL_IOStream *io=SDL_IOFromConstMem(file,size);
    if(io==NULL){
        return NULL;
    }
    SDL_Surface *surface=SDL_LoadSurface_IO(io,true);
    return draft_create_texture_from_surface(align,surface,w,h);
}

draft_texture_t *draft_create_picture(align_t align,const char *path,float w,float h){
    if(path==NULL){
        return NULL;
    }
    SDL_Surface *surface=SDL_LoadSurface(path);
    return draft_create_texture_from_surface(align,surface,w,h);
}

/* ---- 图标 + 文字 ---- */

typedef struct {
    int is_icon;
    SDL_Surface *surface;   /* 文字段；图标段为 NULL，由本函数释放 */
    SDL_Texture *icon;      /* 图标段；借用主题纹理，不释放 */
    int width;
    int height;
}icon_text_part_t;

typedef darray_t(icon_text_part_t) icon_text_parts_t;

static int icon_text_name_start(char c){
    return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';
}

static int icon_text_name_char(char c){
    return icon_text_name_start(c)||(c>='0'&&c<='9');
}

/* 文字段按单行处理：换行符替换成空格，绝不调用 Wrapped 版本 */
static SDL_Surface *icon_text_render_segment(TTF_Font *font,const char *text,size_t length,SDL_Color color){
    if(length==0){
        return NULL;
    }

    char *copy=malloc(length+1);
    if(copy==NULL){
        return NULL;
    }
    for(size_t i=0;i<length;i++){
        char c=text[i];
        copy[i]=(c=='\n'||c=='\r')?' ':c;
    }
    copy[length]='\0';

    SDL_Surface *surface=TTF_RenderText_Blended(font,copy,length,color);
    free(copy);
    return surface;
}

static int icon_text_add_segment(icon_text_parts_t *parts,TTF_Font *font,const char *text,size_t length,SDL_Color color){
    SDL_Surface *surface=icon_text_render_segment(font,text,length,color);
    if(surface==NULL){
        return 0;
    }

    icon_text_part_t part={
        .is_icon=0,
        .surface=surface,
        .icon=NULL,
        .width=surface->w,
        .height=surface->h
    };
    if(da_add(sizeof(part),parts,&part)!=0){
        SDL_DestroySurface(surface);
        return 0;
    }
    return 1;
}

static int icon_text_add_icon(icon_text_parts_t *parts,SDL_Texture *icon,int icon_height){
    if(icon==NULL){
        return 1;   /* 没有可用图标时跳过这个占位符 */
    }

    float texture_w=0.0f;
    float texture_h=0.0f;
    if(!SDL_GetTextureSize(icon,&texture_w,&texture_h)||texture_w<=0.0f||texture_h<=0.0f){
        return 1;
    }

    int height=icon_height;
    int width=(int)SDL_roundf(texture_w*(float)height/texture_h);
    if(width<1){
        width=1;
    }

    icon_text_part_t part={
        .is_icon=1,
        .surface=NULL,
        .icon=icon,
        .width=width,
        .height=height
    };
    return da_add(sizeof(part),parts,&part)==0?1:0;
}

static void icon_text_free_parts(icon_text_parts_t *parts){
    for(size_t i=0;i<parts->size;i++){
        if(parts->arr[i].surface!=NULL){
            SDL_DestroySurface(parts->arr[i].surface);
        }
    }
    da_clear(parts);
}

/* 把 context 拆成文字段和图标段。
 * - \字母/下划线 开头：取最长的 [A-Za-z0-9_]* 作为 name，换成图标
 * - \\：输出一个反斜杠
 * - 末尾单独的 \：忽略
 * - 其他 \x：保留反斜杠，继续当普通文字处理
 * 文字段内部如果有换行符，渲染时替换为空格，保证单行。 */
draft_texture_t *draft_create_icon_with_text(align_t align,TTF_Font *font,char *context,float wl,float hl,unsigned color){
    if(font==NULL||context==NULL||context[0]=='\0'||renderer==NULL){
        return NULL;
    }

    int line_height=TTF_GetFontHeight(font);
    if(line_height<=0){
        return NULL;
    }

    SDL_Color text_color=draft_color(color);
    float scale=sdlgui_get_scale();

    /* 图标高度取 hl（逻辑坐标）；hl<=0 时退回字体行高 */
    int icon_height=line_height;
    if(hl>0.0f){
        icon_height=(int)SDL_roundf(hl*scale);
        if(icon_height<1){
            icon_height=1;
        }
    }

    icon_text_parts_t parts;
    da_init(&parts);
    int has_icon=0;

    const char *p=context;
    const char *literal=p;
    while(*p!='\0'){
        if(*p!='\\'){
            p++;
            continue;
        }

        /* 先把反斜杠之前的文字冲出来 */
        if(p>literal){
            if(!icon_text_add_segment(&parts,font,literal,(size_t)(p-literal),text_color)){
                icon_text_free_parts(&parts);
                return NULL;
            }
        }

        if(p[1]=='\0'){
            /* 末尾单独的反斜杠：跳过 */
            p++;
            literal=p;
            break;
        }

        if(p[1]=='\\'){
            /* 转义反斜杠：只保留一个 */
            if(!icon_text_add_segment(&parts,font,p,1,text_color)){
                icon_text_free_parts(&parts);
                return NULL;
            }
            p+=2;
            literal=p;
            continue;
        }

        if(icon_text_name_start(p[1])){
            const char *name=p+1;
            const char *end=name+1;
            while(icon_text_name_char(*end)){
                end++;
            }
            size_t length=(size_t)(end-name);

            char small[64];
            char *copy=(length<sizeof(small))?small:malloc(length+1);
            if(copy==NULL){
                icon_text_free_parts(&parts);
                return NULL;
            }
            memcpy(copy,name,length);
            copy[length]='\0';

            theme_icon_t icon_index=find_icon(copy);
            if(copy!=small){
                free(copy);
            }

            size_t before=parts.size;
            if(!icon_text_add_icon(&parts,get_icon(icon_index),icon_height)){
                icon_text_free_parts(&parts);
                return NULL;
            }
            if(parts.size>before){
                has_icon=1;
            }

            p=end;
            literal=p;
            continue;
        }

        /* 其他 \x：保留反斜杠，继续按普通文字扫描 */
        p++;
    }

    if(p>literal){
        if(!icon_text_add_segment(&parts,font,literal,(size_t)(p-literal),text_color)){
            icon_text_free_parts(&parts);
            return NULL;
        }
    }

    if(parts.size==0){
        icon_text_free_parts(&parts);
        return NULL;
    }

    float natural_width=0.0f;
    for(size_t i=0;i<parts.size;i++){
        natural_width+=(float)parts.arr[i].width;
    }
    if(natural_width<1.0f){
        icon_text_free_parts(&parts);
        return NULL;
    }

    int target_width=(int)SDL_ceilf(natural_width);
    int target_height=line_height;
    if(has_icon&&icon_height>target_height){
        target_height=icon_height;
    }

    /* wl 仍然限制宽度；不换行，超过时裁剪 */
    if(wl>0.0f){
        float limit=wl*scale;
        if(limit<1.0f){
            limit=1.0f;
        }
        if((float)target_width>limit){
            target_width=(int)SDL_ceilf(limit);
        }
    }
    if(target_width<1||target_height<1){
        icon_text_free_parts(&parts);
        return NULL;
    }

    SDL_Texture *canvas=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,
                                          SDL_TEXTUREACCESS_TARGET,target_width,target_height);
    if(canvas==NULL){
        icon_text_free_parts(&parts);
        return NULL;
    }
    /* 图标/文字以直通 alpha 绘制到透明目标上，目标内容因此是预乘 alpha；
     * 之后按预乘方式混合，避免文字和图标边缘出现深色描边。 */
    SDL_SetTextureBlendMode(canvas,SDL_BLENDMODE_BLEND_PREMULTIPLIED);
    SDL_SetTextureScaleMode(canvas,SDL_SCALEMODE_NEAREST);

    SDL_Texture *previous=SDL_GetRenderTarget(renderer);
    if(!SDL_SetRenderTarget(renderer,canvas)){
        SDL_DestroyTexture(canvas);
        icon_text_free_parts(&parts);
        return NULL;
    }

    SDL_SetRenderDrawColor(renderer,0,0,0,0);
    SDL_RenderClear(renderer);

    float x=0.0f;
    for(size_t i=0;i<parts.size;i++){
        icon_text_part_t *part=&parts.arr[i];
        if(part->is_icon){
            SDL_FRect dst={
                .x=x,
                .y=((float)target_height-(float)part->height)*0.5f,
                .w=(float)part->width,
                .h=(float)part->height
            };
            SDL_RenderTexture(renderer,part->icon,NULL,&dst);
        }else if(part->surface!=NULL){
            SDL_Texture *tex=SDL_CreateTextureFromSurface(renderer,part->surface);
            if(tex!=NULL){
                SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_BLEND);
                /* 文字段按原始像素 1:1 画进合成纹理，不参与线性缩放 */
                SDL_SetTextureScaleMode(tex,SDL_SCALEMODE_NEAREST);
                SDL_FRect dst={
                    .x=x,
                    .y=((float)target_height-(float)part->surface->h)*0.5f,
                    .w=(float)part->surface->w,
                    .h=(float)part->surface->h
                };
                SDL_RenderTexture(renderer,tex,NULL,&dst);
                SDL_DestroyTexture(tex);
            }
        }
        x+=(float)part->width;
    }

    SDL_SetRenderTarget(renderer,previous);
    icon_text_free_parts(&parts);

    draft_texture_t *d=calloc(1,sizeof(*d));
    if(d==NULL){
        SDL_DestroyTexture(canvas);
        return NULL;
    }
    d->draw=draft_draw_texture;
    d->clear=draft_texture_clear;
    d->parent=NULL;
    d->align=align;
    d->tex=canvas;
    d->owns_texture=1;
    d->width=(float)target_width/scale;
    d->height=(float)target_height/scale;
    return d;
}

void draft_draw_texture(void *p){
    draft_texture_t *d=p;
    if(d==NULL||renderer==NULL){
        return;
    }
    draft_update_layout(DRAFT_SUBLAYOUT(d));
    if(d->tex==NULL){
        return;
    }

    float w=d->width;
    float h=d->height;
    if(w<=0.0f||h<=0.0f){
        SDL_GetTextureSize(d->tex,&w,&h);
    }

    /* 纹理按物理像素对齐，避免 0.5px 的居中偏移把文字采样糊 */
    float scale=sdlgui_get_scale();
    if(scale<=0.0f){
        scale=1.0f;
    }
    float x=SDL_roundf(d->x*scale)/scale;
    float y=SDL_roundf(d->y*scale)/scale;
    w=SDL_roundf(w*scale)/scale;
    h=SDL_roundf(h*scale)/scale;
    SDL_FRect dst={.x=x,.y=y,.w=w,.h=h};
    SDL_RenderTexture(renderer,d->tex,NULL,&dst);
}



/* ---- 各判定框的命中判定 ----
   每个子布局结构体的第一个成员都是 bbox_check_func check，指到下面自己的函数。
   签名统一为 int (*)(void *sublayout,float x,float y)：
   x,y 是鼠标的窗口坐标，子布局的 x/y 也已经是窗口绝对坐标
   （相对定位由 layout_update() 按 parent+align 算好，这里不再加偏移）。
   命中返回 TRUE。 */

int bbox_check_rect(void *p,float x,float y){ /* 矩形 */
    bbox_default_t *sl=p;
    /* check 在 SUBLAYOUT_DEF 前面，parent 才是 sublayout_t 的起点 */
    layout_update((sublayout_t *)&sl->parent);

    return x>=sl->x && x<=sl->x+sl->width
        && y>=sl->y && y<=sl->y+sl->height;
}

int bbox_check_triangle(void *p,float x,float y){
    bbox_triangle_t *sl=p;
    layout_update((sublayout_t *)&sl->parent);

    float x1=sl->x+sl->points[0][0], y1=sl->y+sl->points[0][1];
    float x2=sl->x+sl->points[1][0], y2=sl->y+sl->points[1][1];
    float x3=sl->x+sl->points[2][0], y3=sl->y+sl->points[2][1];

    float d1=(x-x1)*(y2-y1)-(y-y1)*(x2-x1);
    float d2=(x-x2)*(y3-y2)-(y-y2)*(x3-x2);
    float d3=(x-x3)*(y1-y3)-(y-y3)*(x1-x3);

    int has_neg=(d1<0.0f)||(d2<0.0f)||(d3<0.0f);
    int has_pos=(d1>0.0f)||(d2>0.0f)||(d3>0.0f);
    return !(has_neg&&has_pos);   /* 三个叉积同号 = 在内部；顶点顺时针逆时针都适用，边上算命中 */
}

int bbox_check_rrect(void *p,float x,float y){ /* 圆角矩形 */
    bbox_rrect_t *sl=p;
    layout_update((sublayout_t *)&sl->parent);

    float w=sl->width, h=sl->height;
    float px=x-sl->x;
    float py=y-sl->y;

    if(px<0.0f||py<0.0f||px>w||py>h){
        return FALSE;          /* 外接矩形粗筛 */
    }

    float r=sl->r;
    float max_r=(w<h?w:h)/2.0f;
    if(sl->auto_radius||r<0.0f){
        r=max_r;               /* 负半径 = 全圆角，取短边的一半 */
    }else if(r>max_r){
        r=max_r;
    }
    if(r<=0.0f){
        return TRUE;           /* 没圆角，退化成普通矩形 */
    }

    /* 内矩形是四条边各内缩 r；落在四个角上的点再按到圆心的距离判圆弧 */
    float cx=(px<r)?r:((px>w-r)?w-r:px);
    float cy=(py<r)?r:((py>h-r)?h-r:py);
    float dx=px-cx, dy=py-cy;
    return dx*dx+dy*dy<=r*r;
}

/* 圆形（判定框是 bbox_default_t）：
   圆要正圆，宽高又可能不相等，所以拆成两个函数，选哪个由调用方按意图定 */

int bbox_check_circle_w(void *p,float x,float y){
    /* 高向宽看齐：直径取 width，圆心在判定框中心（宽>高时圆会上下超出框） */
    bbox_default_t *sl=p;
    layout_update((sublayout_t *)&sl->parent);

    float r=sl->width/2.0f;
    float cx=sl->x+sl->width/2.0f;
    float cy=sl->y+sl->height/2.0f;
    float dx=x-cx, dy=y-cy;
    return dx*dx+dy*dy<=r*r;
}

int bbox_check_circle_h(void *p,float x,float y){
    /* 宽向高看齐：直径取 height，圆心在判定框中心（高>宽时圆会左右超出框） */
    bbox_default_t *sl=p;
    layout_update((sublayout_t *)&sl->parent);

    float r=sl->height/2.0f;
    float cx=sl->x+sl->width/2.0f;
    float cy=sl->y+sl->height/2.0f;
    float dx=x-cx, dy=y-cy;
    return dx*dx+dy*dy<=r*r;
}

bbox_default_t *bbox_create_default(bbox_check_func check,align_t align,float w,float h){
    bbox_default_t *bbox=calloc(1,sizeof(*bbox));
    if(bbox==NULL){
        return NULL;
    }
    bbox->check=check?check:bbox_check_rect;
    bbox->align=align;
    bbox->width=w;
    bbox->height=h;
    return bbox;
}

bbox_triangle_t *bbox_create_triangle(align_t align,float *vertex){
    if(vertex==NULL){
        return NULL;
    }
    bbox_triangle_t *bbox=calloc(1,sizeof(*bbox));
    if(bbox==NULL){
        return NULL;
    }
    bbox->check=bbox_check_triangle;
    bbox->align=align;

    float min_x=vertex[0], min_y=vertex[1];
    float max_x=min_x, max_y=min_y;
    for(size_t i=1;i<3;i++){
        float x=vertex[i*2], y=vertex[i*2+1];
        if(x<min_x){ min_x=x; }
        if(x>max_x){ max_x=x; }
        if(y<min_y){ min_y=y; }
        if(y>max_y){ max_y=y; }
    }
    bbox->x=min_x;
    bbox->y=min_y;
    bbox->width=max_x-min_x;
    bbox->height=max_y-min_y;
    for(size_t i=0;i<3;i++){
        bbox->points[i][0]=vertex[i*2]-min_x;
        bbox->points[i][1]=vertex[i*2+1]-min_y;
    }
    return bbox;
}

bbox_rrect_t *bbox_create_rrect(align_t align,float w,float h,float r){
    bbox_rrect_t *bbox=calloc(1,sizeof(*bbox));
    if(bbox==NULL){
        return NULL;
    }
    int auto_radius=0;
    if(r<0.0f){
        auto_radius=1;
        r=(w<h?w:h)/2.0f;
        if(r<0.0f){
            r=0.0f;
        }
    }
    bbox->check=bbox_check_rrect;
    bbox->align=align;
    bbox->width=w;
    bbox->height=h;
    bbox->r=r;
    bbox->auto_radius=auto_radius?1u:0u;
    return bbox;
}

layout_button_t *create_layout_button(void *parent,float w,float h,align_t align,button_callback_func callback,void *data,...){
    layout_t *p=parent?parent:&root;
    layout_button_t *l=create_layout(sizeof(layout_button_t),p,w,h,NULL,layout_update,button_clear,button_mouse,align);
    if(l==NULL){
        return NULL;
    }

    l->need_bounce=1;
    l->left_last_state=0;
    l->hover_last=0;
    l->pressed_inside=0;
    l->status=BUTTON_STATUS_OUT;
    l->callback=callback;
    l->data=data;
    l->sublayout_resizable=1;
    l->bboxes=NULL;
    l->bboxes_size=0;

    va_list arg;
    va_start(arg,data);

    darray_t(bbox_t*) bboxes;
    da_init(&bboxes);

    bbox_t *bbox=NULL;
    while((bbox=va_arg(arg,bbox_t*))!=NULL){
        bbox->parent=(sublayout_t *)&l->l;
        if(da_add(sizeof(bbox),&bboxes,&bbox)!=0){
            break;
        }
    }

    l->bboxes=bboxes.arr;
    l->bboxes_size=bboxes.size;

    va_end(arg);
    return l;
}

void button_clear(void *p){
    layout_button_t *l=p;
    if(l->sublayout_resizable){
        for(size_t i=0;i<l->bboxes_size;i++){
            free(l->bboxes[i]);
        }
        free(l->bboxes);
        l->bboxes=NULL;
        l->bboxes_size=0;
    }
}

static void button_update_status(layout_button_t *l,button_status_t status){
    if(l->status==status){
        return;
    }
    l->status=status;
    if(l->callback!=NULL){
        l->callback((int)status,l->data);
    }
}

/* 鼠标事件可能发生在 update_sons() 之前，先把自身及祖先布局位置算对 */
static void layout_update_ancestors(sublayout_t *sl){
    if(sl->parent!=NULL){
        layout_update_ancestors(sl->parent);
    }
    layout_update(sl);
}

void button_mouse(void *p){
    layout_button_t *l=p;

    layout_update_ancestors((sublayout_t *)&l->l);

    int hover=FALSE;
    for(size_t i=0;i<l->bboxes_size;i++){
        bbox_t *bbox=l->bboxes[i];
        if(bbox!=NULL&&bbox->check(bbox,get_mouse_x(),get_mouse_y())){
            hover=TRUE;
            break;
        }
    }

    int pressed=get_mouse_left()!=0;
    int was_pressed=l->left_last_state!=0;
    int was_hover=l->hover_last!=0;
    int trigger=FALSE;

    if(pressed&&!was_pressed){
        /* 只有按下那一刻鼠标已经在按钮内，才算按下按钮。
         * 按住左键从按钮外拖进来时 was_hover=0，不能产生 click/confirm。 */
        l->pressed_inside=(hover&&was_hover)?1u:0u;
        if(l->need_bounce==0&&hover&&was_hover){
            trigger=TRUE;
        }
    }else if(!pressed&&was_pressed){
        if(l->need_bounce==1&&l->pressed_inside){
            trigger=TRUE;
        }else if(l->need_bounce==2&&l->pressed_inside&&hover){
            trigger=TRUE;
        }
        l->pressed_inside=0;
    }

    l->left_last_state=pressed?1u:0u;
    l->hover_last=hover?1u:0u;

    if(trigger){
        button_update_status(l,BUTTON_STATUS_TRIGGER);
    }else if(hover){
        button_update_status(l,BUTTON_STATUS_HOVER);
    }else{
        button_update_status(l,BUTTON_STATUS_OUT);
    }
    /* 子布局的鼠标遍历由 layout_mouse() 统一继续，这里不递归 */
}




/* 释放 animation 拥有的 draft：先调用各自的 clear，再释放 draft 本身 */
static void draft_destroy(draft_t *d){
    if(d==NULL){
        return;
    }
    if(d->clear!=NULL){
        d->clear(d);
    }
    free(d);
}

layout_animation_t *create_layout_animation(void *p,animate_func animation,...){
    va_list arg;
    va_start(arg,animation);

    align_t align={
        .horizon=3,
        .vertical=3,
        .width=1,
        .height=1,
        .horizon_offset=0.0f,
        .vertical_offset=0.0f,
        .width_offset=1.0f,
        .height_offset=1.0f
    };

    layout_t *parent=p;
    if(parent==NULL){
        parent=&root;
    }

    float width=parent->width;
    float height=parent->height;

    layout_animation_t *ret=create_layout(
        sizeof(layout_animation_t),
        parent,
        width,
        height,
        animation_draw,
        layout_update,
        animation_clear,
        NULL,
        align
    );
    if(ret==NULL){
        va_end(arg);
        return NULL;
    }

    ret->animation=animation;
    ret->lasting=0;
    ret->status=0;
    ret->draft_size=0;
    ret->draft_resizable=1;

    darray_t(draft_t*) drafts;
    da_init(&drafts);

    draft_t *d=NULL;
    while((d=va_arg(arg,draft_t*))!=NULL){
        d->parent=(sublayout_t *)&ret->l;
        if(da_add(sizeof(d),&drafts,&d)!=0){
            break;
        }
    }

    ret->draft_size=drafts.size;
    ret->drafts=drafts.arr;

    va_end(arg);
    return ret;
}

void animation_draw(void *p){
    layout_animation_t *l=p;
    if(l==NULL){
        return;
    }

    if((l->status||l->lasting)&&l->animation){
        if(!l->lasting){
            l->timer=SDL_GetTicks();
        }
        l->animation(l);
    }
    if(l->drafts==NULL){
        return;
    }
    for(size_t i=0;i<l->draft_size;i++){
        draft_t *draft=l->drafts[i];
        if(draft==NULL){
            continue;
        }
        draw_func draw=draft->draw;
        if(draw!=NULL){
            draw(draft);
        }
    }
}

void animation_clear(void *p){
    layout_animation_t *l=p;
    if(l==NULL){
        return;
    }

    for(size_t i=0;i<l->draft_size;i++){
        if(l->drafts){
            draft_destroy(l->drafts[i]);
        }
    }
    if(l->draft_resizable){
        free(l->drafts);
    }
    l->drafts=NULL;
    l->draft_size=0;
    l->animation=NULL;
    l->status=0;
    l->timer=0;
}


#define LIST_WHEEL_STEP 32.0f

/* list 的子布局存放在自己的 sons 里，不进入 layout_t.sons，
 * 所以通用的 update_sons()/layout_mouse()/draw_root() 不会遍历它们。
 * 每个子布局的 parent 指向前一个子布局，第一个子布局的 parent 临时指向
 * list 自身；list 自己负责从当前起点开始按可见性更新/绘制/处理鼠标。 */

static float list_item_extent(const layout_list_t *l,const layout_t *item){
    return l->direction?item->width:item->height;
}

static float list_view_extent(const layout_list_t *l){
    return l->direction?l->l.width:l->l.height;
}

static float list_content_extent(const layout_list_t *l){
    float total=0.0f;
    for(size_t i=0;i<l->sons.size;i++){
        total+=list_item_extent(l,l->sons.arr[i]);
    }
    return total;
}

static float list_scroll_offset(const layout_list_t *l){
    float offset=l->start;
    if(l->i>l->sons.size){
        return offset;
    }
    for(size_t i=0;i<l->i;i++){
        offset+=list_item_extent(l,l->sons.arr[i]);
    }
    return offset;
}

/* 把绝对滚动量换算回 (i,start)：i 是第一个可见子布局，
 * start 是它相对 list 起点已经滚过去的距离。 */
static void list_set_scroll_offset(layout_list_t *l,float offset){
    if(l->sons.size==0){
        l->i=0;
        l->start=0.0f;
        return;
    }
    if(offset<0.0f){
        offset=0.0f;
    }

    size_t i=0;
    while(i+1<l->sons.size){
        float extent=list_item_extent(l,l->sons.arr[i]);
        if(extent>0.0f&&offset>=extent){
            offset-=extent;
            i++;
        }else{
            break;
        }
    }
    l->i=i;
    l->start=offset;
}

static void list_refresh_slidable(layout_list_t *l){
    float content=list_content_extent(l);
    float view=list_view_extent(l);
    l->can_be_slided=(content>view+0.5f)?1:0;
    if(!l->can_be_slided){
        l->i=0;
        l->start=0.0f;
    }
}

static void list_clamp_scroll(layout_list_t *l,float offset){
    float content=list_content_extent(l);
    float view=list_view_extent(l);
    float max_offset=content-view;
    if(max_offset<0.0f){
        max_offset=0.0f;
    }
    if(offset<0.0f){
        offset=0.0f;
    }else if(offset>max_offset){
        offset=max_offset;
    }
    list_set_scroll_offset(l,offset);
}

static int list_item_after_view(const layout_list_t *l,const layout_t *item){
    if(l->direction){
        return item->x>=l->l.x+l->l.width;
    }
    return item->y>=l->l.y+l->l.height;
}

static int list_item_before_view(const layout_list_t *l,const layout_t *item){
    if(l->direction){
        return item->x+item->width<=l->l.x;
    }
    return item->y+item->height<=l->l.y;
}

/* 第一个子布局对齐到 list：沿延伸方向再减去 start。 */
static void list_set_start_align(layout_list_t *l,layout_t *item){
    if(l->direction==0){
        item->align.horizon=2;
        item->align.horizon_offset=item->width;
        item->align.vertical=2;
        item->align.vertical_offset=item->height-l->start;
    }else{
        item->align.horizon=2;
        item->align.horizon_offset=item->width-l->start;
        item->align.vertical=2;
        item->align.vertical_offset=item->height;
    }
}

/* 后续子布局对齐到前一个：横向排就放在右边，纵向排就放在下边。 */
static void list_set_next_align(layout_list_t *l,layout_t *item){
    if(l->direction==0){
        item->align.horizon=2;
        item->align.horizon_offset=item->width;   /* x = 前一个的 x */
        item->align.vertical=1;
        item->align.vertical_offset=0.0f;         /* y = 前一个的下边 */
    }else{
        item->align.horizon=1;
        item->align.horizon_offset=0.0f;          /* x = 前一个的右边 */
        item->align.vertical=2;
        item->align.vertical_offset=item->height; /* y = 前一个的 y */
    }
}

int list_add(layout_list_t *l,layout_t *item){
    if(l==NULL||item==NULL){
        return 0;
    }

    if(da_add(sizeof(item),&l->sons,&item)!=0){
        return 0;
    }

    layout_detach(item);
    size_t index=l->sons.size-1;
    if(index==0){
        item->parent=(sublayout_t *)&l->l;
        list_set_start_align(l,item);
    }else{
        item->parent=(sublayout_t *)l->sons.arr[index-1];
        list_set_next_align(l,item);
    }
    return 1;
}

static int list_mouse_inside(const layout_list_t *l){
    float x=get_mouse_x();
    float y=get_mouse_y();
    return x>=l->l.x&&x<=l->l.x+l->l.width
        && y>=l->l.y&&y<=l->l.y+l->l.height;
}

static void list_handle_wheel(layout_list_t *l){
    if(l->sons.size==0||!list_mouse_inside(l)){
        return;
    }
    list_refresh_slidable(l);
    if(!l->can_be_slided){
        return;
    }

    float delta;
    if(l->direction){
        /* 触摸板横向滚动优先；普通滚轮竖向滚动也映射为横向。 */
        delta=get_mouse_wheel_x()-get_mouse_wheel_y();
    }else{
        delta=-get_mouse_wheel_y();
    }
    if(delta==0.0f){
        return;
    }

    list_clamp_scroll(l,list_scroll_offset(l)+delta*LIST_WHEEL_STEP);
    mouse_wheel[0]=0.0f;
    mouse_wheel[1]=0.0f;
}

/* 把从 i 开始的可见子布局排好。start 子布局的 parent 临时指向 list，
 * 后续子布局按 parent 链依次对齐；一旦某个子布局完全离开视口就停止。 */
static void list_begin(layout_list_t *l){
    if(l->sons.size==0){
        return;
    }
    if(l->l.parent!=NULL){
        layout_update(&l->l);
    }

    list_refresh_slidable(l);
    if(l->i>=l->sons.size){
        l->i=l->sons.size-1;
    }
    list_clamp_scroll(l,list_scroll_offset(l));

    layout_t *prev=NULL;
    for(size_t i=l->i;i<l->sons.size;i++){
        layout_t *item=l->sons.arr[i];
        if(i==l->i){
            item->parent=(sublayout_t *)&l->l;
            list_set_start_align(l,item);
        }else{
            item->parent=(sublayout_t *)prev;
            list_set_next_align(l,item);
        }

        if(item->update!=NULL){
            item->update(item);
        }else{
            layout_update(item);
        }
        update_sons(item);
        prev=item;

        if(list_item_after_view(l,item)){
            break;
        }
    }
}

/* start 子布局在正常链里的 parent 是前一个；操作结束后恢复，
 * 下次它作为后续子布局时仍能按链定位。 */
static void list_end(layout_list_t *l){
    if(l->sons.size==0||l->i==0||l->i>=l->sons.size){
        return;
    }
    l->sons.arr[l->i]->parent=(sublayout_t *)l->sons.arr[l->i-1];
}

layout_list_t *create_layout_list(void *parent,float w,float h,align_t align,int direction,...){
    layout_t *parent_layout=parent?parent:&root;
    layout_list_t *l=create_layout(
        sizeof(layout_list_t),parent_layout,w,h,list_draw,list_update,list_clear,list_mouse,align);
    if(l==NULL){
        return NULL;
    }

    l->direction=direction?1u:0u;
    l->can_be_slided=0;
    da_init(&l->sons);
    l->i=0;
    l->start=0.0f;

    va_list arg;
    va_start(arg,direction);

    layout_t *layout=NULL;
    while((layout=va_arg(arg,layout_t*))!=NULL){
        if(da_add(sizeof(layout_t*),&l->sons,&layout)!=0){
            break;
        }

        /* 子布局从原 parent 摘下，改由 list 自己持有；不再挂到 l->l.sons。 */
        layout_detach(layout);
        size_t index=l->sons.size-1;

        if(index==0){
            layout->parent=(sublayout_t *)&l->l;
            list_set_start_align(l,layout);
        }else{
            layout->parent=(sublayout_t *)l->sons.arr[index-1];
            list_set_next_align(l,layout);
        }
    }

    va_end(arg);
    list_refresh_slidable(l);
    return l;
}

void list_clear(void *p){
    layout_list_t *l=p;
    if(l==NULL){
        return;
    }
    for(size_t i=0;i<l->sons.size;i++){
        delete_layout(l->sons.arr[i]);
    }
    da_clear(&l->sons);
    l->i=0;
    l->start=0.0f;
    l->can_be_slided=0;
}

void list_slide(layout_list_t *l,float off){
    if(l==NULL||!l->can_be_slided||off==0.0f){
        return;
    }
    list_clamp_scroll(l,list_scroll_offset(l)+off);
}

float list_get_slide(const layout_list_t *l){
    if(l==NULL||!l->can_be_slided){
        return 0.0f;
    }
    return list_scroll_offset(l);
}

float list_get_content_height(const layout_list_t *l){
    return l!=NULL?list_content_extent(l):0.0f;
}

void list_set_slide(layout_list_t *l,float offset){
    if(l==NULL||!l->can_be_slided){
        return;
    }
    list_clamp_scroll(l,offset);
}

void list_update(void *p){
    layout_list_t *l=p;
    if(l==NULL){
        return;
    }
    list_begin(l);
    list_end(l);
}

void list_draw(void *p){
    layout_list_t *l=p;
    if(l==NULL||l->sons.size==0){
        return;
    }

    list_begin(l);
    for(size_t i=l->i;i<l->sons.size;i++){
        layout_t *item=l->sons.arr[i];
        if(list_item_after_view(l,item)){
            break;
        }
        if(!list_item_before_view(l,item)){
            draw_layout(item);
        }
    }
    list_end(l);
}

void list_mouse(void *p){
    layout_list_t *l=p;
    if(l==NULL){
        return;
    }

    layout_update_ancestors((sublayout_t *)&l->l);

    int inside=list_mouse_inside(l);
    if(inside){
        list_handle_wheel(l);
    }

    list_begin(l);

    for(size_t i=l->i;i<l->sons.size;i++){
        layout_t *item=l->sons.arr[i];
        if(list_item_after_view(l,item)){
            break;
        }
        if(list_item_before_view(l,item)){
            continue;
        }
        layout_mouse_dispatch(item);
    }
    list_end(l);

    /* 只有可滚动的 list 才消费滚轮；can_be_slided=0 时留给其他布局 */
    if(inside&&l->can_be_slided){
        mouse_wheel[0]=0.0f;
        mouse_wheel[1]=0.0f;
    }
}

/* ---- input layout：鼠标形状/焦点/键盘编辑，文字 draft 只绘制文字 ---- */

static int draft_text_build_cache(draft_text_t *d);
static void input_sync_ime_area(layout_input_t *input);

#define INPUT_SCROLL_STEP 48.0f

/* 截断到 max_bytes，并保证不切断 UTF-8 字符。max_bytes=0 表示不限制。 */
static size_t input_limit_prefix(const char *text,size_t length,size_t max_bytes){
    if(text==NULL||max_bytes==0||length<=max_bytes){
        return length;
    }
    size_t limit=max_bytes;
    while(limit>0&&((unsigned char)text[limit]&0xC0)==0x80){
        limit--;
    }
    return limit;
}

static void input_measure_text(layout_input_t *input){
    if(input==NULL){
        return;
    }
    input->text_width=0.0f;
    if(input->font==NULL||input->text==NULL||input->length==0){
        return;
    }

    int width=0;
    int height=0;
    if(TTF_GetStringSize(input->font,input->text,input->length,&width,&height)&&width>0){
        float scale=sdlgui_get_scale();
        if(scale<=0.0f){
            scale=1.0f;
        }
        input->text_width=(float)width/scale;
    }else if(input->text_draft!=NULL){
        input->text_width=input->text_draft->width;
    }
}

static void input_clamp_scroll(layout_input_t *input){
    if(input==NULL){
        return;
    }
    float max_scroll=input->text_width-input->l.width;
    if(max_scroll<0.0f){
        max_scroll=0.0f;
    }
    if(input->text_scroll<0.0f){
        input->text_scroll=0.0f;
    }else if(input->text_scroll>max_scroll){
        input->text_scroll=max_scroll;
    }
}

static void input_position_text(layout_input_t *input){
    if(input==NULL||input->text_draft==NULL){
        return;
    }

    float scale=sdlgui_get_scale();
    if(scale<=0.0f){
        scale=1.0f;
    }
    float line_height=input->font!=NULL?(float)TTF_GetFontHeight(input->font)/scale:0.0f;

    input->text_draft->x=input->l.x-input->text_scroll;
    input->text_draft->y=input->l.y;
    if(input->text_draft->height>0.0f){
        input->text_draft->y+=(input->l.height-input->text_draft->height)/2.0f;
    }else if(line_height>0.0f){
        input->text_draft->y+=(input->l.height-line_height)/2.0f;
    }
}

static int input_trim_to_limit(layout_input_t *input){
    if(input==NULL||input->max_bytes==0||input->length<=input->max_bytes){
        return 0;
    }

    size_t length=input_limit_prefix(input->text,input->length,input->max_bytes);
    input->length=length;
    input->text[length]='\0';
    if(input->cursor>length){
        input->cursor=length;
    }
    return 1;
}

static void input_rebuild_text(layout_input_t *input){
    if(input==NULL){
        return;
    }

    if(input->text_draft!=NULL){
        input->text_draft->context=input->text;
        input->text_draft->can_enter=0;
        input->text_draft->can_change=0;
        draft_text_clear(input->text_draft);
        draft_text_build_cache(input->text_draft);
    }

    input_measure_text(input);
    input_clamp_scroll(input);
    input_position_text(input);
}

static int input_reserve(layout_input_t *input,size_t need){
    if(input==NULL){
        return 0;
    }
    if(need<=input->capacity){
        return 1;
    }

    size_t capacity=input->capacity>0?input->capacity:32;
    while(capacity<need){
        if(capacity>SIZE_MAX/2){
            capacity=need;
            break;
        }
        capacity*=2;
    }

    char *text=realloc(input->text,capacity);
    if(text==NULL){
        return 0;
    }
    input->text=text;
    input->capacity=capacity;
    return 1;
}

static size_t input_utf8_prev(const char *text,size_t at){
    if(text==NULL||at==0){
        return 0;
    }
    at--;
    while(at>0&&((unsigned char)text[at]&0xC0)==0x80){
        at--;
    }
    return at;
}

static size_t input_utf8_next(const char *text,size_t length,size_t at){
    if(text==NULL||at>=length){
        return length;
    }
    at++;
    while(at<length&&((unsigned char)text[at]&0xC0)==0x80){
        at++;
    }
    return at;
}

static int input_insert(layout_input_t *input,const char *text){
    if(input==NULL){
        return 0;
    }

    int trimmed=input_trim_to_limit(input);

    size_t add=(text!=NULL)?strlen(text):0;
    if(input->max_bytes>0&&add>0){
        size_t available=input->max_bytes-input->length;
        if(add>available){
            add=available>0?input_limit_prefix(text,add,available):0;
        }
    }

    if(add==0){
        if(trimmed){
            input_rebuild_text(input);
        }
        return 1;
    }

    if(!input_reserve(input,input->length+add+1)){
        return 0;
    }

    memmove(input->text+input->cursor+add,
        input->text+input->cursor,
        input->length-input->cursor+1);
    memcpy(input->text+input->cursor,text,add);
    input->length+=add;
    input->cursor+=add;
    input_rebuild_text(input);
    return 1;
}

static void input_clear(void *p){
    layout_input_t *input=p;
    if(input==NULL){
        return;
    }

    if(focused_input==input){
        layout_input_blur();
    }
    if(input_hover==input){
        SDL_SetCursor(SDL_GetDefaultCursor());
        input_hover=NULL;
    }

    if(input->text_draft!=NULL){
        draft_text_clear(input->text_draft);
        free(input->text_draft);
        input->text_draft=NULL;
    }

    free(input->text);
    input->text=NULL;
    input->length=0;
    input->capacity=0;
    input->cursor=0;
    input->text_width=0.0f;
    input->text_scroll=0.0f;
}

static void input_mouse(void *p){
    layout_input_t *input=p;
    if(input==NULL){
        return;
    }

    int inside=layout_contains_mouse(&input->l);
    int left=get_mouse_left();

    if(inside){
        if(input_hover!=input){
            if(input_text_cursor==NULL){
                input_text_cursor=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
            }
            if(input_text_cursor!=NULL){
                SDL_SetCursor(input_text_cursor);
            }
            input_hover=input;
        }
        if(left&&!input->last_left){
            layout_input_focus(input);
        }

        if(input->text_width>input->l.width&&input->l.width>0.0f){
            float wheel=get_mouse_wheel_x();
            if(wheel!=0.0f){
                input->text_scroll+=wheel*INPUT_SCROLL_STEP;
                input_clamp_scroll(input);
                mouse_wheel[0]=0.0f;
                input_position_text(input);
                input_sync_ime_area(input);
            }
        }
    }else if(input_hover==input){
        SDL_SetCursor(SDL_GetDefaultCursor());
        input_hover=NULL;
    }

    input->last_left=left?1:0;
}

static float input_prefix_width(const layout_input_t *input){
    if(input==NULL||input->font==NULL||input->text==NULL||input->cursor==0){
        return 0.0f;
    }

    /* TTF_GetStringSize(...,0,...) 会测量整段字符串，传入 0 会得到
     * “光标在末尾”的宽度，必须单独处理 cursor==0。 */
    int width=0;
    int height=0;
    if(!TTF_GetStringSize(input->font,input->text,input->cursor,&width,&height)||width<=0){
        return 0.0f;
    }

    float scale=sdlgui_get_scale();
    if(scale<=0.0f){
        scale=1.0f;
    }
    return (float)width/scale;
}

/* 把 IME/候选窗的位置同步到输入框和当前光标。
 * SDL3 的 SDL_SetTextInputArea() 会交给平台输入法使用；
 * rect 是窗口逻辑坐标，cursor 是相对 rect.x 的偏移。 */
static void input_sync_ime_area(layout_input_t *input){
    SDL_Window *w=get_window();
    if(input==NULL||!input->focused||w==NULL){
        return;
    }

    /* 把区域缩到光标所在的小矩形。macOS 的 SDL Cocoa 后端直接用这个
     * rect 决定候选窗位置，不读取 cursor 偏移；给整块输入框会让候选窗
     * 停在输入框左下角，所以这里传光标位置。 */
    float caret_x=input->l.x+input_prefix_width(input)-input->text_scroll;
    if(caret_x<input->l.x){
        caret_x=input->l.x;
    }
    if(caret_x>input->l.x+input->l.width){
        caret_x=input->l.x+input->l.width;
    }

    float scale=sdlgui_get_scale();
    if(scale<=0.0f){
        scale=1.0f;
    }
    float line_height=input->font!=NULL?(float)TTF_GetFontHeight(input->font)/scale:0.0f;
    if(line_height<=0.0f){
        line_height=input->l.height;
    }
    if(line_height>input->l.height){
        line_height=input->l.height;
    }

    SDL_Rect area={
        (int)SDL_floorf(caret_x),
        (int)SDL_floorf(input->l.y+(input->l.height-line_height)/2.0f),
        1,
        (int)SDL_ceilf(line_height)
    };
    SDL_SetTextInputArea(w,&area,0);
}

void layout_input_focus(layout_input_t *input){
    if(input==NULL||focused_input==input){
        return;
    }

    if(focused_input!=NULL){
        focused_input->focused=0;
    }
    focused_input=input;
    input->focused=1;

    if(get_window()!=NULL){
        SDL_StartTextInput(get_window());
        input_sync_ime_area(input);
    }
}

void layout_input_blur(void){
    if(focused_input==NULL){
        return;
    }

    focused_input->focused=0;
    focused_input=NULL;

    if(get_window()!=NULL){
        SDL_SetTextInputArea(get_window(),NULL,0);
        SDL_StopTextInput(get_window());
    }
}

const char *layout_input_text(const layout_input_t *input){
    if(input==NULL||input->text==NULL){
        return "";
    }
    return input->text;
}

size_t layout_input_cursor(const layout_input_t *input){
    return input!=NULL?input->cursor:0;
}

int layout_input_is_focused(const layout_input_t *input){
    return input!=NULL&&input->focused;
}

int layout_input_event(const SDL_Event *event){
    if(event==NULL||focused_input==NULL){
        return 0;
    }

    layout_input_t *input=focused_input;

    if(event->type==SDL_EVENT_TEXT_INPUT){
        input_insert(input,event->text.text);
        return 1;
    }
    if(event->type!=SDL_EVENT_KEY_DOWN){
        return 0;
    }

    switch(event->key.key){
    case SDLK_ESCAPE:
        layout_input_blur();
        return 1;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        if(input->callback!=NULL){
            input->callback(layout_input_text(input),input->data);
        }
        return 1;
    case SDLK_BACKSPACE:
        if(input->cursor>0){
            size_t prev=input_utf8_prev(input->text,input->cursor);
            memmove(input->text+prev,
                input->text+input->cursor,
                input->length-input->cursor+1);
            input->length-=input->cursor-prev;
            input->cursor=prev;
            input_rebuild_text(input);
        }
        return 1;
    case SDLK_DELETE:
        if(input->cursor<input->length){
            size_t next=input_utf8_next(input->text,input->length,input->cursor);
            memmove(input->text+input->cursor,
                input->text+next,
                input->length-next+1);
            input->length-=next-input->cursor;
            input_rebuild_text(input);
        }
        return 1;
    case SDLK_LEFT:
        input->cursor=input_utf8_prev(input->text,input->cursor);
        return 1;
    case SDLK_RIGHT:
        input->cursor=input_utf8_next(input->text,input->length,input->cursor);
        return 1;
    case SDLK_HOME:
        input->cursor=0;
        return 1;
    case SDLK_END:
        input->cursor=input->length;
        return 1;
    case SDLK_V:
        if((event->key.mod&(SDL_KMOD_CTRL|SDL_KMOD_GUI))!=0){
            char *clipboard=SDL_GetClipboardText();
            if(clipboard!=NULL){
                input_insert(input,clipboard);
                SDL_free(clipboard);
            }
            return 1;
        }
        break;
    default:
        break;
    }

    return 0;
}

static void input_update(void *p){
    layout_input_t *input=p;
    if(input==NULL){
        return;
    }

    layout_update(&input->l);
    if(input_trim_to_limit(input)){
        input_rebuild_text(input);
    }
    input_measure_text(input);
    input_clamp_scroll(input);
    input_position_text(input);
    input_sync_ime_area(input);
}

static void input_draw(void *p){
    layout_input_t *input=p;
    if(input==NULL||input->text_draft==NULL||renderer==NULL){
        return;
    }

    input_update(input);
    draft_draw_text(input->text_draft);
}

layout_input_t *create_layout_input(void *parent,float w,float h,align_t align,
    const char *text,TTF_Font *font,unsigned color,size_t max_bytes,
    input_callback_func callback,void *data){
    if(w<=0.0f||h<=0.0f||font==NULL){
        return NULL;
    }

    layout_input_t *input=(layout_input_t *)create_layout(
        sizeof(*input),
        parent,
        w,
        h,
        input_draw,
        input_update,
        input_clear,
        input_mouse,
        align
    );
    if(input==NULL){
        return NULL;
    }

    size_t full_length=text!=NULL?strlen(text):0;
    size_t length=input_limit_prefix(text,full_length,max_bytes);
    input->text=malloc(length+1);
    if(input->text==NULL){
        delete_layout((layout_t *)input);
        return NULL;
    }

    if(length>0){
        memcpy(input->text,text,length);
    }
    input->text[length]='\0';
    input->length=length;
    input->capacity=length+1;
    input->cursor=length;
    input->max_bytes=max_bytes;
    input->font=font;
    input->text_color=color;
    input->callback=callback;
    input->data=data;
    input->focused=0;
    input->last_left=0;

    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    input->text_draft=draft_create_text(zero,font,input->text,0.0f,0.0f,color);
    if(input->text_draft==NULL){
        delete_layout((layout_t *)input);
        return NULL;
    }
    input->text_draft->parent=(sublayout_t *)&input->l;
    input->text_draft->can_enter=0;
    input->text_draft->can_change=0;
    input_rebuild_text(input);
    return input;
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

    unsigned background=get_color(COLOR_MAIN);
    SDL_SetRenderDrawColor(renderer,
        (background>>24)&0xFF,(background>>16)&0xFF,
        (background>>8)&0xFF,background&0xFF);
    SDL_RenderClear(renderer);

    update_root();
    draw_root();

    SDL_RenderPresent(renderer);
    rendering=0;
}

static bool SDLCALL sdlgui_live_resize_watch(void *userdata,SDL_Event *event){
    (void)userdata;
    if(event->type!=SDL_EVENT_WINDOW_EXPOSED){
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
