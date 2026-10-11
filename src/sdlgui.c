#include <SDL3/SDL.h>
#include "sdlgui.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#define TRUE  1
#define FALSE 0


SDL_Window *window=NULL;
SDL_Renderer *renderer=NULL;
layout_t root={.son_resizable=1,.update=update_layout};


void sdlgui_init(const char *title,int width,int height,const char *iconpath){
    if(!SDL_Init(SDL_INIT_VIDEO)){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"SDL_Init failed: %s",SDL_GetError());
        exit(1);
    }

    if(!SDL_CreateWindowAndRenderer(title,width,height,SDL_WINDOW_RESIZABLE,&window,&renderer)){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Create window/renderer failed: %s",SDL_GetError());
        SDL_Quit();
        exit(1);
    }

    SDL_SetRenderVSync(renderer,1);


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
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
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


void draw_rect(void *p){
    rect_layout_t *rect=p;
    layout_t *l=&rect->l;
    SDL_FRect box={.x=l->x,.y=l->y,.w=l->width,.h=l->height};
    SDL_SetRenderDrawColor(renderer,
        (Uint8)((rect->color>>24)&0xFF),
        (Uint8)((rect->color>>16)&0xFF),
        (Uint8)((rect->color>>8)&0xFF),
        (Uint8)(rect->color&0xFF));
    SDL_RenderFillRect(renderer,&box);
}

void *create_layout(size_t siz,layout_t *parent,float w,float h,draw_func draw,update_func update,clear_func clear,align_t align){
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
    new->son_resizable=1;  // sons 数组是 realloc 出来的，可以继续增删、需要 free
    new->need_free=1;      // 本结构体是 malloc 出来的，delete_layout 时要 free
    new->align_type=align;
    new->parent=parent;
    new->width=w;
    new->height=h;
    new->draw=draw;
    new->update=update;
    new->clear=clear;

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
        }
    }
}

void update_layout_only(void *p){
    layout_t *l=p;
    layout_t *parent=l->parent;

    switch(l->align_type.width){
    case 1:
        l->width = parent->width * l->align_type.width_offset;
        break;
    case 2:
        l->width = parent->width + l->align_type.width_offset;
        break;
    }

    switch(l->align_type.horizon){
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

    if(l->align_type.horizon){
        l->x+=l->align_type.horizon_offset;
    }



    switch(l->align_type.height){
    case 1:
        l->height = parent->height * l->align_type.height_offset;
        break;
    case 2:
        l->height = parent->height + l->align_type.height_offset;
        break;
    }

    switch(l->align_type.vertical){
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

    if(l->align_type.vertical){
        l->y+=l->align_type.vertical_offset;
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