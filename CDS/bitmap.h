#ifndef BITMAP_H
#define BITMAP_H

#include <stddef.h>

typedef struct {
    unsigned *data;
    size_t size;
}bitmap_t;

void bm_init(bitmap_t *bitmap,size_t size);
void bm_resize(bitmap_t *bitmap,size_t size);
void bm_clear(bitmap_t *bitmap);
void bm_fake_clear(bitmap_t *bitmap);

int bm_get(bitmap_t *bitmap,size_t i);
void bm_set(bitmap_t *bitmap,size_t i,int v);
void bm_push(bitmap_t *bitmap,int v);

#ifdef BITMAP_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

#ifdef BITMAP_DEBUG_OUTPUT
#include <stdio.h>
#include <errno.h>
#define bm_log_err(...) fprintf(stderr,__VA_ARGS__)
#else
#define bm_log_err(...) ((void)0)
#endif

#define NOT_LESS_THAN(size,n) (((size)%(n))?(size)/(n)+1:(size)/(n))

void bm_init(bitmap_t *bitmap,size_t size){
    bitmap->data=NULL;
    bitmap->size=0;
    if(size==0){
        return ;
    }
    unsigned *data=malloc(NOT_LESS_THAN(size,32)*sizeof(unsigned));
    if(!data){
        bm_log_err("bm_init: malloc %zu bytes failed: %s\n",(size_t)(NOT_LESS_THAN(size,32)*sizeof(unsigned)),strerror(errno));
        return ;
    }
    bitmap->data=data;
    bitmap->size=size;
    memset(bitmap->data,0,NOT_LESS_THAN(size,32)*sizeof(unsigned));
}

void bm_resize(bitmap_t *bitmap,size_t size){
    unsigned *data=realloc(bitmap->data,NOT_LESS_THAN(size,32)*sizeof(unsigned));
    if(!data&&size){
        bm_log_err("bm_resize: realloc %zu bytes failed: %s\n",(size_t)(NOT_LESS_THAN(size,32)*sizeof(unsigned)),strerror(errno));
        return ;
    }
    bitmap->data=data;
    if(size>bitmap->size){
        memset(bitmap->data+NOT_LESS_THAN(bitmap->size,32),0,(NOT_LESS_THAN(size,32)-NOT_LESS_THAN(bitmap->size,32))*sizeof(unsigned));
    }
    bitmap->size=size;
}

void bm_clear(bitmap_t *bitmap){
    free(bitmap->data);
    bitmap->data=NULL;
    bitmap->size=0;
}

void bm_fake_clear(bitmap_t *bitmap){
    bitmap->size=0;
}


int bm_get(bitmap_t *bitmap,size_t i){
    if(i>=bitmap->size){
        return -1;
    }
    return bitmap->data[i/32]&(1u<<(i%32))?1:0;
}
void bm_set(bitmap_t *bitmap,size_t i,int v){
    if(i>=bitmap->size){
        return ;
    }
    unsigned n;
    if(v==0){
        n=1u<<(i%32);
        n=~n;
        bitmap->data[i/32]=bitmap->data[i/32]&n;
    }else{
        n=1u<<(i%32);
        bitmap->data[i/32]=bitmap->data[i/32]|n;
    }
}

void bm_push(bitmap_t *bitmap,int v){
    size_t size=bitmap->size+1;
    if(!size){
        return ;
    }
    if(NOT_LESS_THAN(size,32)>NOT_LESS_THAN(bitmap->size,32)){
        bm_resize(bitmap,size);
        if(bitmap->size<size){
            return ;
        }
    }else{
        bitmap->size=size;
    }
    bm_set(bitmap,size-1,v);
}

#undef bm_log_err

#endif

#endif
