#include <SDL3/SDL.h>
#include "sdlgui_widget.h"
#include <stdlib.h>


/* timer 是已过去的时间，whole 是总时长；返回 origin -> target 的插值颜色 */
static unsigned color_gradient(unsigned target,unsigned origin,size_t timer,size_t whole){
    if(whole==0||timer>=whole){
        return target;
    }
    if(timer==0){
        return origin;
    }

    float t=(float)timer/(float)whole;
    unsigned tr=(target>>24)&0xFF, tg=(target>>16)&0xFF, tb=(target>>8)&0xFF, ta=target&0xFF;
    unsigned or=(origin>>24)&0xFF, og=(origin>>16)&0xFF, ob=(origin>>8)&0xFF, oa=origin&0xFF;

    unsigned r=(unsigned)((float)or+((float)tr-(float)or)*t+0.5f);
    unsigned g=(unsigned)((float)og+((float)tg-(float)og)*t+0.5f);
    unsigned b=(unsigned)((float)ob+((float)tb-(float)ob)*t+0.5f);
    unsigned a=(unsigned)((float)oa+((float)ta-(float)oa)*t+0.5f);
    return (r<<24)|(g<<16)|(b<<8)|a;
}

static void button_animation(void *a){
    layout_animation_t *l=a;
    if(l==NULL||l->drafts==NULL||l->draft_size==0){
        return;
    }

    draft_rrect_t *draft=(draft_rrect_t *)l->drafts[0];
    if(draft==NULL){
        return;
    }

    unsigned target=get_color(COLOR_BUTTON);
    if(l->status==BUTTON_STATUS_HOVER){
        target=get_color(COLOR_BUTTON_HIGHLIGHT);
    }else if(l->status==BUTTON_STATUS_TRIGGER){
        target=get_color(COLOR_BUTTON_CONFIRM);
        l->lasting=1;
    }

    if(l->lasting){
        unsigned long now=SDL_GetTicks();
        unsigned color=color_gradient(target,get_color(COLOR_BUTTON),now-l->timer,get_theme()->button_animation_time);
        draft->color=color;
        draft->border_color=color;
        if(now-l->timer>get_theme()->button_animation_time){
            l->lasting=0;
        }
    }else{
        draft->color=target;
        draft->border_color=target;
    }
}

struct button_callbacl_data{
    layout_animation_t *animation;
    button_callback_func callback;
    void *data;
};

static void widget_clear(void *p){
    widget_button_t *widget=p;
    free(widget->callback_data);
    widget->callback_data=NULL;
}

static void button_callback(int status,void *data){
    struct button_callbacl_data *tmp=data;

    tmp->animation->status=status;
    if(tmp->animation->status!=BUTTON_STATUS_TRIGGER||SDL_GetTicks()-tmp->animation->timer>get_theme()->button_animation_time){\
        tmp->animation->timer=SDL_GetTicks();
    }
    if(tmp->animation->animation!=NULL){
        tmp->animation->animation(tmp->animation);
    }

    if(status==BUTTON_STATUS_TRIGGER&&tmp->callback!=NULL){
        tmp->callback(status,tmp->data);
    }
}

widget_button_t *widget_create_button(void *parent,float width,align_t align,const char *text,button_callback_func callback,void *data){
    float height=get_theme()->button_height;
    if(get_theme()->item_height>0.0f&&height>get_theme()->item_height){
        height=get_theme()->item_height;
    }
    if(width<=0.0f||height<=0.0f){
        return NULL;
    }

    widget_button_t *widget=create_layout(
        sizeof(*widget),
        parent,
        width,
        height,
        NULL,
        layout_update,
        widget_clear,
        layout_mouse,
        align
    );
    if(widget==NULL){
        return NULL;
    }

    align_t full=make_align(3,3,1,1,0,0,1.0f,1.0f);

    layout_animation_t *animation=create_layout_animation(widget,button_animation,
        draft_create_rrect(full,width,height,get_theme()->rect_round_corner,get_color(COLOR_BUTTON),get_color(COLOR_BUTTON)),
        draft_create_text(
            make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f),
            obtain_font(get_font(TEXT_CONTEXT),
            get_text_size(TEXT_CONTEXT)),
            (char *)text,
            width,
            height,
            get_text_color(TEXT_CONTEXT)
        ),
        NULL
    );

    struct button_callbacl_data *tmp=calloc(1,sizeof(struct button_callbacl_data));
    if(tmp==NULL){
        return widget;
    }
    tmp->animation=animation;
    tmp->callback=callback;
    tmp->data=data;
    widget->callback_data=tmp;

    bbox_rrect_t *bbox=bbox_create_rrect(full,width,height,get_theme()->rect_round_corner);
    layout_button_t *button=create_layout_button(widget,width,height,full,button_callback,tmp,bbox,NULL);
    (void)button;

    return widget;
}
