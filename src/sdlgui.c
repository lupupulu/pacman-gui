#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "sdlgui.h"
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#define TRUE  1
#define FALSE 0


static SDL_Window *window=NULL;
static SDL_Renderer *renderer=NULL;
static layout_t root={.son_resizable=1,.update=update_layout};


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

    if(!theme_load_default_font()){
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
    theme_quit();
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



layout_rect_t *create_layout_rect(void *parent,float w,float h,align_t align){
    return create_layout(sizeof(layout_rect_t),parent,w,h,rect_draw,update_layout,NULL,NULL,align);
}



/* ---- 绘制草稿（draft） ----
   draft 不是 layout，不进入 layout 的 sons 树；它由 create 分配，由
   layout_draw_t 之类的容器按 draw 函数指针调用。parent 留给容器挂接时填写，
   未挂接时按根布局定位；draw 里会用 align 重新算一次 x/y/宽高。 */

#define DRAFT_SUBLAYOUT(d) ((sublayout_t *)&(d)->parent)

static void draft_update_layout(sublayout_t *sl){
    if(sl->parent==NULL){
        sl->parent=(sublayout_t *)&root;
    }
    update_layout_only(sl);
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

/* 实心圆：逐行画水平线，避免引入额外的几何库 */
static void draft_fill_circle(float cx,float cy,float radius,SDL_Color color){
    if(radius<=0.0f){
        return;
    }
    SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,color.a);
    int r=(int)(radius+0.5f);
    for(int dy=-r;dy<=r;dy++){
        float dx=SDL_sqrtf((float)(r*r-dy*dy));
        SDL_RenderLine(renderer,cx-dx,cy+(float)dy,cx+dx,cy+(float)dy);
    }
}

/* 实心圆角矩形：同样逐行画水平线，四条圆角按半径内缩 */
static void draft_fill_rrect(float x,float y,float w,float h,float r,SDL_Color color){
    if(w<=0.0f||h<=0.0f){
        return;
    }
    float max_r=(w<h?w:h)/2.0f;
    if(r>max_r){ r=max_r; }
    if(r<0.0f){ r=0.0f; }

    SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,color.a);
    for(float yy=0.5f;yy<h;yy+=1.0f){
        float inset=0.0f;
        if(r>0.0f){
            if(yy<r){
                float dy=r-yy;
                inset=r-SDL_sqrtf(r*r-dy*dy);
            }else if(yy>h-r){
                float dy=yy-(h-r);
                inset=r-SDL_sqrtf(r*r-dy*dy);
            }
        }
        SDL_RenderLine(renderer,x+inset,y+yy,x+w-inset,y+yy);
    }
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
    d->draw=draft_draw_rrect;
    d->parent=NULL;
    d->align=align;
    d->width=w;
    d->height=h;
    d->r=r;
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

    if(draft_has_border(d->border_color)){
        draft_fill_rrect(d->x,d->y,d->width,d->height,d->r,draft_color(d->border_color));
        if(d->width>2.0f&&d->height>2.0f){
            draft_fill_rrect(d->x+1.0f,d->y+1.0f,d->width-2.0f,d->height-2.0f,
                             d->r-1.0f,draft_color(d->color));
        }
    }else{
        draft_fill_rrect(d->x,d->y,d->width,d->height,d->r,draft_color(d->color));
    }
}

draft_text_t *draft_create_text(align_t align,char *context,float wl,float hl,float hpl,unsigned color){
    draft_text_t *d=calloc(1,sizeof(*d));
    if(d==NULL){
        return NULL;
    }
    d->draw=draft_draw_text;
    d->parent=NULL;
    d->align=align;
    d->width=wl;
    d->height=hl;
    d->can_enter=1;
    d->can_change=1;
    d->context=context;
    d->width_limit=wl;
    d->height_per_line=hpl;
    d->height_limit=hl;
    d->color=color;
    return d;
}

void draft_draw_text(void *p){
    draft_text_t *d=p;
    if(d==NULL||renderer==NULL){
        return;
    }
    draft_update_layout(DRAFT_SUBLAYOUT(d));
    if(d->context==NULL||d->context[0]=='\0'){
        return;
    }

    TTF_Font *base=get_font(0);
    if(base==NULL){
        return;
    }

    float ptsize=d->height_per_line>0.0f?d->height_per_line:get_text_size(0);
    if(ptsize<=0.0f){
        return;
    }

    TTF_Font *font=TTF_CopyFont(base);
    if(font==NULL){
        return;
    }
    if(!TTF_SetFontSize(font,ptsize)){
        TTF_CloseFont(font);
        return;
    }

    SDL_Color color=draft_color(d->color);
    size_t length=strlen(d->context);
    SDL_Surface *surface=NULL;
    if(d->width_limit>0.0f){
        surface=TTF_RenderText_Blended_Wrapped(font,d->context,length,color,
                                               (int)(d->width_limit+0.5f));
    }else{
        surface=TTF_RenderText_Blended(font,d->context,length,color);
    }
    TTF_CloseFont(font);

    if(surface==NULL){
        return;
    }

    SDL_Texture *tex=SDL_CreateTextureFromSurface(renderer,surface);
    if(tex!=NULL){
        float w=(float)surface->w;
        float h=(float)surface->h;
        if(d->height_limit>0.0f&&h>d->height_limit){
            h=d->height_limit;
        }
        SDL_FRect src={.x=0,.y=0,.w=w,.h=h};
        SDL_FRect dst={.x=d->x,.y=d->y,.w=w,.h=h};
        SDL_RenderTexture(renderer,tex,&src,&dst);
        SDL_DestroyTexture(tex);
    }
    SDL_DestroySurface(surface);
}

static draft_texture_t *draft_create_texture(align_t align,SDL_Surface *surface){
    if(surface==NULL){
        return NULL;
    }
    if(renderer==NULL){
        SDL_DestroySurface(surface);
        return NULL;
    }

    draft_texture_t *d=calloc(1,sizeof(*d));
    if(d==NULL){
        SDL_DestroySurface(surface);
        return NULL;
    }
    d->draw=draft_draw_texture;
    d->parent=NULL;
    d->align=align;
    d->tex=SDL_CreateTextureFromSurface(renderer,surface);
    SDL_DestroySurface(surface);
    if(d->tex==NULL){
        free(d);
        return NULL;
    }

    if(!SDL_GetTextureSize(d->tex,&d->width,&d->height)){
        SDL_DestroyTexture(d->tex);
        free(d);
        return NULL;
    }
    return d;
}

draft_texture_t *draft_create_picture_from_memory(align_t align,const char *file,size_t size){
    if(file==NULL||size==0){
        return NULL;
    }
    SDL_IOStream *io=SDL_IOFromConstMem(file,size);
    if(io==NULL){
        return NULL;
    }
    SDL_Surface *surface=SDL_LoadSurface_IO(io,true);
    return draft_create_texture(align,surface);
}

draft_texture_t *draft_create_picture(align_t align,const char *path){
    if(path==NULL){
        return NULL;
    }
    SDL_Surface *surface=SDL_LoadSurface(path);
    return draft_create_texture(align,surface);
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
    SDL_FRect dst={.x=d->x,.y=d->y,.w=w,.h=h};
    SDL_RenderTexture(renderer,d->tex,NULL,&dst);
}



/* ---- 各判定框的命中判定 ----
   每个子布局结构体的第一个成员都是 check_func check，指到下面自己的函数。
   签名统一为 int (*)(void *sublayout,float x,float y)：
   x,y 是鼠标的窗口坐标，子布局的 x/y 也已经是窗口绝对坐标
   （相对定位由 update_layout_only() 按 parent+align 算好，这里不再加偏移）。
   命中返回 TRUE。 */

int check_rect(void *p,float x,float y){ /* 矩形 */
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



layout_button_t *create_layout_button(void *parent,float w,float h,align_t align,int need_bounce,button_callback_func callback,void *data){
    layout_button_t *l=create_layout(sizeof(layout_button_t),parent,w,h,NULL,update_layout,button_clear,button_mouse,align);

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
