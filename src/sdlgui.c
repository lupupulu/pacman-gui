#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "sdlgui.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>

#define TRUE  1
#define FALSE 0


static SDL_Window *window=NULL;
static SDL_Renderer *renderer=NULL;
static layout_t root={.son_resizable=1,.update=update_layout};

/* theme_t 里的 color/text 是 unsigned *，内建主题需要各自准备静态后备数组。 */
static unsigned builtin_bright_color[]={0xFFFFFFFF,0x7F7F7FFF,0x1E90FFFF,0x1E90FFFF};
static unsigned builtin_bright_text_color[]={0x000000FF};
static unsigned builtin_dark_color[]={0x000000FF,0x7F7F7FFF,0x3399FFFF,0x1E90FFFF};
static unsigned builtin_dark_text_color[]={0x000000FF};
static unsigned builtin_text_size[]={20,16,14};

/* 默认嵌入字体：由 tools/ttf_to_c.py 生成到 build/pacman_pixel_font.c。 */
extern const unsigned char pacman_pixel_ttf[];
extern const size_t pacman_pixel_ttf_len;

static theme_slice_t builtin_theme_slice[]={
    {
        .name="Bright",
        .color=builtin_bright_color,
        .color_size=4,
        .text_color=builtin_bright_text_color,
        .text_color_size=1
    },
    {
        .name="Dark",
        .color=builtin_dark_color,
        .color_size=4,
        .text_color=builtin_dark_text_color,
        .text_color_size=1
    }
};

static theme_t theme={
    .version="0.0.1",
    .name=NULL,
    .now_slice=&builtin_theme_slice[0],

    .fonts=NULL,
    .font_size=0,

    .text=builtin_text_size,
    .text_size=3,

    .item_height=32.0f,
    .rect_round_corner=0.0f,

    .slices=builtin_theme_slice,
    .slice_size=2
};

static float mouse_pos[]={0,0};   /* 鼠标在窗口内的位置 */
static int mouse_buttons[]={0,0,0,0,0};   /* 左 中 右 X1 X2，1=按下 0=未按下 */



void set_mouse_pos(float x,float y){
    mouse_pos[0]=x;
    mouse_pos[1]=y;
}
float get_mouse_x(void){
    return mouse_pos[0];
}
float get_mouse_y(void){
    return mouse_pos[1];
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


/* 关闭并释放默认主题里的字体。必须在 TTF_Quit() 之前调用。 */
static void free_theme_fonts(void){
    if(theme.fonts!=NULL){
        for(size_t i=0;i<theme.font_size;i++){
            if(theme.fonts[i]!=NULL){
                TTF_CloseFont(theme.fonts[i]);
            }
        }
        free(theme.fonts);
        theme.fonts=NULL;
    }
    theme.font_size=0;
}

/* 把嵌入的默认字体打开一份，挂到默认主题上。
 * fonts 是字体族列表（默认配置里只有一个），text 是各级字号；
 * 这里用第一个字号打开，后续渲染可按需 TTF_SetFontSize()。 */
static int load_default_font(void){
    float ptsize=theme.text[0];
    if(theme.text!=NULL&&theme.text_size>0){
        ptsize=(float)theme.text[0];
    }else{
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"No text sizes configured, using 20 pt");
    }

    TTF_Font **fonts=calloc(1,sizeof(*fonts));
    if(fonts==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"calloc fonts failed: %s",SDL_GetError());
        return 0;
    }

    SDL_IOStream *io=SDL_IOFromConstMem(pacman_pixel_ttf,pacman_pixel_ttf_len);
    if(io==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"SDL_IOFromConstMem failed: %s",SDL_GetError());
        free(fonts);
        return 0;
    }

    fonts[0]=TTF_OpenFontIO(io,true,ptsize);
    if(fonts[0]==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"TTF_OpenFontIO(%.0f) failed: %s",ptsize,SDL_GetError());
        free(fonts);
        return 0;
    }

    theme.fonts=fonts;
    theme.font_size=1;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"Loaded embedded default font at %.0f pt",ptsize);
    return 1;
}

void sdlgui_init(const char *title,int width,int height,const char *iconpath){
    if(!SDL_Init(SDL_INIT_VIDEO)||!TTF_Init()){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"SDL_Init failed: %s",SDL_GetError());
        exit(1);
    }

    if(!SDL_CreateWindowAndRenderer(title,width,height,SDL_WINDOW_RESIZABLE,&window,&renderer)){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Create window/renderer failed: %s",SDL_GetError());
        SDL_Quit();
        exit(1);
    }

    SDL_SetRenderVSync(renderer,1);

    if(!load_default_font()){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Failed to load the embedded default font");
        sdlgui_quit();
        exit(1);
    }

    if(iconpath!=NULL){
        SDL_Surface *icon=SDL_LoadSurface(iconpath);
        if(!icon){
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Icon load failed: %s",SDL_GetError());
        }else{
            if(!SDL_SetWindowIcon(window,icon)){
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Set icon failed: %s",SDL_GetError());
            }
            SDL_DestroySurface(icon);
        }
    }


}
void sdlgui_quit(void){
    delete_layout(&root);
    free_theme_fonts();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
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
    int w,h;
    if(!SDL_GetWindowSizeInPixels(window,&w,&h)){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Get size of window failed: %s",SDL_GetError());
        return;
    }
    root.width=(float)w;
    root.height=(float)h;

    update_sons(&root);
}
const layout_t *get_root(void){
    return &root;
}

float get_root_w(void){
    return root.width;
}
float get_root_h(void){
    return root.height;
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
    if(parent==NULL){
        parent=&root;
    }

    if(parent->son_resizable==FALSE){
        return NULL;
    }

    layout_t *new=malloc(siz);
    if(new==NULL){
        return NULL;
    }

    memset(new,0,siz);

    layout_t **sons=realloc(parent->sons,(parent->son_size+1)*sizeof(layout_t*));
    if(sons==NULL){
        free(new);
        return NULL;
    }
    parent->sons=sons;
    parent->sons[parent->son_size]=new;
    parent->son_size++;

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
        }else{
            update_sons(son[i]);
        }
    }
}

void mouse_update_sons(void *p){
    layout_t *l=p;

    layout_t **son=l->sons;
    for(size_t i=0;i<l->son_size;i++){
        if(!son[i]->enable||!son[i]->active){
            continue;
        }
        if(son[i]->mouse){
            son[i]->mouse(son[i]);
        }else{
            mouse_update_sons(son[i]);
        }
    }
}

void update_layout_only(void *p){
    sublayout_t *l=p;
    sublayout_t *parent=l->parent;

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

void update_layout(void *p){
    layout_t *l=p;
    update_layout_only(l);
    if(l->draw){
        l->draw(l);
    }
    update_sons(l);
}




/* ---- 各判定框的命中判定 ----
   每个子布局结构体的第一个成员都是 check_func check，指到下面自己的函数。
   签名统一为 int (*)(void *sublayout,float x,float y)：
   x,y 是鼠标的窗口坐标，子布局的 x/y 也已经是窗口绝对坐标
   （相对定位由 update_layout_only() 按 parent+align 算好，这里不再加偏移）。
   命中返回 TRUE。 */

int check_default(void *p,float x,float y){ /* 矩形 */
    default_sublayout_t *sl=p;
    update_layout_only(&sl->align);

    return x>=sl->x && x<=sl->x+sl->width
        && y>=sl->y && y<=sl->y+sl->height;
}

int check_triangle(void *p,float x,float y){
    triangle_sublayout_t *sl=p;
    float x1=sl->points[0][0], y1=sl->points[0][1];
    float x2=sl->points[1][0], y2=sl->points[1][1];
    float x3=sl->points[2][0], y3=sl->points[2][1];

    float d1=(x-x1)*(y2-y1)-(y-y1)*(x2-x1);
    float d2=(x-x2)*(y3-y2)-(y-y2)*(x3-x2);
    float d3=(x-x3)*(y1-y3)-(y-y3)*(x1-x3);

    int has_neg=(d1<0.0f)||(d2<0.0f)||(d3<0.0f);
    int has_pos=(d1>0.0f)||(d2>0.0f)||(d3>0.0f);
    return !(has_neg&&has_pos);   /* 三个叉积同号 = 在内部；顶点顺时针逆时针都适用，边上算命中 */
}

int check_rrect(void *p,float x,float y){ /* 圆角矩形 */
    rrect_sublayout_t *sl=p;
    update_layout_only(&sl->align);

    float w=sl->width, h=sl->height;
    float px=x-sl->x;
    float py=y-sl->y;

    if(px<0.0f||py<0.0f||px>w||py>h){
        return FALSE;          /* 外接矩形粗筛 */
    }

    float r=sl->r;
    if(r>w/2.0f){ r=w/2.0f; }  /* 半径超过一半就按最大圆角算 */
    if(r>h/2.0f){ r=h/2.0f; }
    if(r<=0.0f){
        return TRUE;           /* 没圆角，退化成普通矩形 */
    }

    /* 内矩形是四条边各内缩 r；落在四个角上的点再按到圆心的距离判圆弧 */
    float cx=(px<r)?r:((px>w-r)?w-r:px);
    float cy=(py<r)?r:((py>h-r)?h-r:py);
    float dx=px-cx, dy=py-cy;
    return dx*dx+dy*dy<=r*r;
}

/* 圆形（判定框是 default_sublayout_t）：
   圆要正圆，宽高又可能不相等，所以拆成两个函数，选哪个由调用方按意图定 */

int check_circle_w(void *p,float x,float y){
    /* 高向宽看齐：直径取 width，圆心在判定框中心（宽>高时圆会上下超出框） */
    default_sublayout_t *sl=p;
    update_layout_only(&sl->align);

    float r=sl->width/2.0f;
    float cx=sl->x+sl->width/2.0f;
    float cy=sl->y+sl->height/2.0f;
    float dx=x-cx, dy=y-cy;
    return dx*dx+dy*dy<=r*r;
}

int check_circle_h(void *p,float x,float y){
    /* 宽向高看齐：直径取 height，圆心在判定框中心（高>宽时圆会左右超出框） */
    default_sublayout_t *sl=p;
    update_layout_only(&sl->align);

    float r=sl->height/2.0f;
    float cx=sl->x+sl->width/2.0f;
    float cy=sl->y+sl->height/2.0f;
    float dx=x-cx, dy=y-cy;
    return dx*dx+dy*dy<=r*r;
}


layout_button_t *button_init(layout_button_t *l,int need_bounce,button_callback_func callback,void *data){
    l->need_bounce=need_bounce;

    l->callback=callback;
    l->data=data;

    l->sublayout_resizable=1;
    l->sublayouts=NULL;
    l->sublayouts_size=0;

    return l;
}

void button_clear(void *p){
    layout_button_t *l=p;
    if(l->sublayout_resizable){
        free(l->sublayouts);
        l->sublayouts=NULL;
        l->sublayouts_size=0;
    }
}

void button_mouse(void *p){
    layout_button_t *l=p;

    if(l->need_bounce==1&&l->left_last_state==1&&get_mouse_left()==0){
        if(l->callback){
            l->callback(l->data);
        }
        l->left_last_state=FALSE;
        return ;
    }

    if(l->need_bounce!=2&&get_mouse_left()==0){
        return ;
    }

    int click=FALSE;
    for(size_t i=0;i<l->sublayouts_size;i++){
        /* sublayouts[i] 指向子布局结构体，第一个成员就是它自己的 check 函数 */
        check_func check=*(check_func*)l->sublayouts[i];
        if(check&&check(l->sublayouts[i],get_mouse_x(),get_mouse_y())){
            click=TRUE;
            break;
        }
    }

    if(l->need_bounce==2&&l->left_last_state==1&&get_mouse_left()==0){
        if(l->callback){
            l->callback(l->data);
        }
        l->left_last_state=FALSE;
        return ;
    }

    l->left_last_state=click;

    mouse_update_sons(p);
}
