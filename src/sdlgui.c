#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <fontconfig/fontconfig.h>
#include "sdlgui.h"
#include "sdlgui_i18n.h"
#include "CDS/confc.h"
#include <locale.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include "CDS/darray.h"

#define TRUE  1
#define FALSE 0

void sdlgui_i18n_init(void){
    setlocale(LC_ALL,"");
    bindtextdomain(SDLGUI_TEXT_DOMAIN,SDLGUI_LOCALEDIR);
    bind_textdomain_codeset(SDLGUI_TEXT_DOMAIN,"UTF-8");
    textdomain(SDLGUI_TEXT_DOMAIN);
}


static SDL_Window *window=NULL;
static SDL_Renderer *renderer=NULL;
static layout_t root={.son_resizable=1,.update=layout_update};

static int fontconfig_ready=0;


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


/* 在 SDLGUI_THEME_PATH 中查找 <name>/config.toml，返回 malloc 路径 */
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

/* theme 是主题名；找不到指定主题时回退 Default，再回退内置主题 */
static void load_theme_by_name(const char *theme){
    const char *theme_path=getenv("SDLGUI_THEME_PATH");
    if(theme_path==NULL||theme_path[0]=='\0'){
        theme_path=SDLGUI_THEME_PATH;
    }

    const char *name=(theme!=NULL&&theme[0]!='\0')?theme:"Default";
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

void sdlgui_init(const char *title,int width,int height,const char *iconpath,const char *theme){
    /* 内建主题只使用 C locale；加载主题后再由 theme_set() 启用 gettext */
    setlocale(LC_ALL,"C");
    if(!SDL_Init(SDL_INIT_VIDEO)||!TTF_Init()){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("SDL_Init failed: %s"),SDL_GetError());
        exit(1);
    }

    if(!SDL_CreateWindowAndRenderer(title,width,height,SDL_WINDOW_RESIZABLE,&window,&renderer)){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Create window/renderer failed: %s"),SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        exit(1);
    }

    SDL_SetRenderVSync(renderer,1);

    if(!FcInit()){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("FcInit failed"));
        sdlgui_quit();
        exit(1);
    }
    fontconfig_ready=1;

    if(!theme_load_default_font()){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to load the embedded default font"));
        sdlgui_quit();
        exit(1);
    }

    /* theme 是主题名：先查指定主题，再查 Default，最后保持内置主题 */
    load_theme_by_name(theme);

    /* 内置主题也跟随系统亮/暗；加载主题后 theme_set() 会再应用一次 */
    theme_select_system();

    if(iconpath!=NULL){
        SDL_Surface *icon=SDL_LoadSurface(iconpath);
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
    theme_quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    if(fontconfig_ready){
        FcFini();
        fontconfig_ready=0;
    }
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Get size of window failed: %s"),SDL_GetError());
        return;
    }
    root.width=(float)w;
    root.height=(float)h;

    layout_mouse(&root);
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
        }else{
            update_sons(son[i]);
        }
    }
}

void layout_mouse(void *p){
    layout_t *l=p;

    layout_t **son=l->sons;
    for(size_t i=0;i<l->son_size;i++){
        if(!son[i]->enable||!son[i]->active){
            continue;
        }
        if(son[i]->mouse){
            son[i]->mouse(son[i]);
        }else{
            layout_mouse(son[i]);
        }
    }
}

void update_layout_only(void *p){
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

void layout_update(void *p){
    layout_t *l=p;
    if(l==NULL){
        return;
    }
    if(l->parent){
        update_layout_only(l);
    }
    if(l->draw){
        l->draw(l);
    }
    if(l->x<get_root_w()*2&&l->y<get_root_h()*2&&l->x+l->width>-get_root_w()*2&&l->y+l->height>-get_root_h()*2){
        update_sons(l);
    }
}



layout_rect_t *create_layout_rect(void *parent,float w,float h,align_t align){
    layout_t *p=parent?parent:&root;
    return create_layout(sizeof(layout_rect_t),p,w,h,rect_draw,layout_update,NULL,NULL,align);
}



/* ---- 绘制草稿（draft） ----
   draft 不是 layout，不进入 layout 的 sons 树；它由 create 分配，由
   layout_draw_t 之类的容器按 draw 函数指针调用。parent 留给容器挂接时填写，
   未挂接时按根布局定位；draw 里会用 align 重新算一次 x/y/宽高。 */

#define DRAFT_SUBLAYOUT(d) ((sublayout_t *)&(d)->parent)

static void draft_text_clear_entry(void *p);
static void draft_texture_clear_entry(void *p);
static int draft_text_build_cache(draft_text_t *d);

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
            draft_fill_rrect(d->x+1.0f,d->y+1.0f,d->width-2.0f,d->height-2.0f,d->r-1.0f,draft_color(d->color));
        }
    }else{
        draft_fill_rrect(d->x,d->y,d->width,d->height,d->r,draft_color(d->color));
    }
}

draft_text_t *draft_create_text(align_t align,TTF_Font *font,char *context,float wl,float hl,unsigned color){
    draft_text_t *d=calloc(1,sizeof(*d));
    if(d==NULL){
        return NULL;
    }
    d->draw=draft_draw_text;
    d->clear=draft_text_clear_entry;
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
void draft_text_clear(draft_text_t *d){
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

static void draft_text_clear_entry(void *p){
    draft_text_clear((draft_text_t *)p);
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
        return TTF_RenderText_Blended_Wrapped(d->font,text,length,color,(int)(d->width_limit+0.5f));
    }
    return TTF_RenderText_Blended(d->font,text,length,color);
}

/* 计算最终渲染宽高；height_limit>0 时高度按裁剪后的结果算 */
static int draft_text_measure_size(draft_text_t *d,float *out_width,float *out_height){
    if(d->font==NULL||d->context==NULL||d->context[0]=='\0'){
        return 0;
    }
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

    int final_height=total_height;
    if(d->height_limit>0.0f&&(float)final_height>d->height_limit){
        final_height=(int)(d->height_limit+0.5f);
    }
    if(final_height<=0){
        return 0;
    }

    *out_width=(float)max_width;
    *out_height=(float)final_height;
    return 1;
}

/* can_change=0：把整段文本合成为一张纹理，之后只做 blit */
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

    /* 第一遍：算总宽高 */
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

    d->cache=cache;
    d->cache_width=(float)max_width;
    d->cache_height=(float)total_height;

    int final_height=total_height;
    if(d->height_limit>0.0f&&(float)final_height>d->height_limit){
        final_height=(int)(d->height_limit+0.5f);
    }
    d->width=(float)max_width;
    d->height=(float)final_height;
    return 1;
}

/* can_change!=0：每帧重新画，不保留缓存 */
static void draft_text_draw_direct(draft_text_t *d){
    int line_height=TTF_GetFontHeight(d->font);
    if(line_height<=0){
        return;
    }

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
        float advance=(float)line_height;
        if(surface!=NULL){
            float draw_height=(float)surface->h;
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
                SDL_FRect src={.x=0,.y=0,.w=(float)surface->w,.h=draw_height};
                SDL_FRect dst={.x=d->x,.y=y,.w=(float)surface->w,.h=draw_height};
                SDL_RenderTexture(renderer,tex,&src,&dst);
                SDL_DestroyTexture(tex);
            }
            if(d->width_limit>0.0f){
                advance=(float)surface->h;
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

    SDL_FRect src={.x=0,.y=0,.w=width,.h=height};
    SDL_FRect dst={.x=d->x,.y=d->y,.w=width,.h=height};
    SDL_RenderTexture(renderer,d->cache,&src,&dst);
}

void draft_texture_clear(draft_texture_t *d){
    if(d==NULL){
        return;
    }
    if(d->tex!=NULL){
        SDL_DestroyTexture(d->tex);
        d->tex=NULL;
    }
}

static void draft_texture_clear_entry(void *p){
    draft_texture_clear((draft_texture_t *)p);
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
    d->clear=draft_texture_clear_entry;
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
   每个子布局结构体的第一个成员都是 bbox_check_func check，指到下面自己的函数。
   签名统一为 int (*)(void *sublayout,float x,float y)：
   x,y 是鼠标的窗口坐标，子布局的 x/y 也已经是窗口绝对坐标
   （相对定位由 update_layout_only() 按 parent+align 算好，这里不再加偏移）。
   命中返回 TRUE。 */

int bbox_check_rect(void *p,float x,float y){ /* 矩形 */
    bbox_default_t *sl=p;
    /* check 在 SUBLAYOUT_DEF 前面，parent 才是 sublayout_t 的起点 */
    update_layout_only((sublayout_t *)&sl->parent);

    return x>=sl->x && x<=sl->x+sl->width
        && y>=sl->y && y<=sl->y+sl->height;
}

int bbox_check_triangle(void *p,float x,float y){
    bbox_triangle_t *sl=p;
    update_layout_only((sublayout_t *)&sl->parent);

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
    update_layout_only((sublayout_t *)&sl->parent);

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

/* 圆形（判定框是 bbox_default_t）：
   圆要正圆，宽高又可能不相等，所以拆成两个函数，选哪个由调用方按意图定 */

int bbox_check_circle_w(void *p,float x,float y){
    /* 高向宽看齐：直径取 width，圆心在判定框中心（宽>高时圆会上下超出框） */
    bbox_default_t *sl=p;
    update_layout_only((sublayout_t *)&sl->parent);

    float r=sl->width/2.0f;
    float cx=sl->x+sl->width/2.0f;
    float cy=sl->y+sl->height/2.0f;
    float dx=x-cx, dy=y-cy;
    return dx*dx+dy*dy<=r*r;
}

int bbox_check_circle_h(void *p,float x,float y){
    /* 宽向高看齐：直径取 height，圆心在判定框中心（高>宽时圆会左右超出框） */
    bbox_default_t *sl=p;
    update_layout_only((sublayout_t *)&sl->parent);

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
    bbox->check=bbox_check_rrect;
    bbox->align=align;
    bbox->width=w;
    bbox->height=h;
    bbox->r=r;
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
    for(size_t i=0;i<l->bboxes_size;i++){
        /* bboxes[i] 指向子布局结构体，第一个成员就是它自己的 check 函数 */
        bbox_t *bbox=l->bboxes[i];
        if(bbox&&bbox->check(bbox,get_mouse_x(),get_mouse_y())){
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

    layout_mouse(p);
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
        layout_mouse,
        align
    );
    if(ret==NULL){
        va_end(arg);
        return NULL;
    }

    ret->animation=animation;
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

    if(l->status&&l->animation){
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


layout_list_t *create_layout_list(void *parent,float w,float h,align_t align,int direction,...){
    layout_t *parent_layout=parent?parent:&root;
    layout_list_t *l=create_layout(
        sizeof(layout_list_t),parent_layout,w,h,NULL,layout_update,NULL,layout_mouse,align);
    if(l==NULL){
        return NULL;
    }

    l->direction=direction?1u:0u;
    l->limited=0;
    l->i=0;
    l->start=0.0f;

    va_list arg;
    va_start(arg,direction);

    layout_t *layout=NULL;
    while((layout=va_arg(arg,layout_t*))!=NULL){
        /* 从原 parent 摘下并挂到 list，孩子所有权归 list */
        if(!layout_attach(layout,(layout_t *)l)){
            continue;
        }
        if(l->direction==0){
            layout->align.vertical=1;
            layout->align.vertical_offset=layout->height;
        }else{
            layout->align.horizon=2;
            layout->align.horizon_offset=layout->width;
        }
    }

    va_end(arg);
    return l;
}
