#ifndef DARRAY_H
#define DARRAY_H

#if !(defined (__GNUG__)&&defined(size_t))
typedef __SIZE_TYPE__ size_t;
#ifdef __BEOS__
typedef long ssize_t;
#endif
#endif

#define darray_t(Tp) \
struct{\
    Tp *arr;\
    size_t size,real;\
}

int da_init(void *array);
int da_add(size_t tp_size,void *array,const void *buf);
int da_add_loc(size_t tp_size,void *array,const void *buf,size_t i);
int da_del(size_t tp_size,void *array,size_t i);
int da_resize(size_t tp_size,void *array,size_t size);
int da_fake_pop(size_t tp_size,void *array);
int da_pop(size_t tp_size,void *array);
int da_fake_clear(void *array);
int da_clear(void *array);

#ifdef DARRAY_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef DARRAY_DEBUG_OUTPUT
#include <errno.h>
#define da_log_err(...) fprintf(stderr,__VA_ARGS__)
#else
#define da_log_err(...) ((void)0)
#endif

/* 动态数组头：不直接按具体元素类型去别名访问调用方的 darray_t(T)，
   而是用 memcpy 读写，避免 -O2/-O3 下严格别名优化导致的错误。 */
typedef struct {
    void *arr;
    size_t size;
    size_t real;
} da_generic_t;

static void da_load_header(const void *array,da_generic_t *header){
    memcpy(header,array,sizeof(*header));
}
static void da_store_header(void *array,const da_generic_t *header){
    memcpy(array,header,sizeof(*header));
}

int da_init(void *array){
    memset(array,0,sizeof(da_generic_t));
    return 0;
}

int da_add(size_t tp_size,void *array,const void *buf){
    da_generic_t header;
    da_load_header(array,&header);
    if(header.size+1>=header.real){
        size_t real=header.real*3/2+1;
        void *p=realloc(header.arr,real*tp_size);
        if(!p){
            da_log_err("da_add: realloc %zu bytes failed: %s\n",real*tp_size,strerror(errno));
            return 127;
        }
        header.arr=p;
        header.real=real;
    }
    memcpy((char *)header.arr+header.size*tp_size,buf,tp_size);
    header.size++;
    da_store_header(array,&header);
    return 0;
}

int da_add_loc(size_t tp_size,void *array,const void *buf,size_t i){
    da_generic_t header;
    da_load_header(array,&header);
    if(i>header.size){
        return 127;
    }
    if(i==header.size){
        return da_add(tp_size,array,buf);
    }
    if(da_add(tp_size,array,buf)!=0){
        return 127;
    }
    da_load_header(array,&header);
    memmove((char *)header.arr+tp_size*(i+1),(char *)header.arr+tp_size*i,tp_size*(header.size-i-1));
    memcpy((char *)header.arr+tp_size*i,buf,tp_size);
    return 0;
}

int da_del(size_t tp_size,void *array,size_t i){
    da_generic_t header;
    da_load_header(array,&header);
    if(!header.size||i>=header.size){
        return 0;
    }else if(header.size-1==i){
        return da_pop(tp_size,array);
    }
    memmove((char *)header.arr+tp_size*i,(char *)header.arr+tp_size*(i+1),tp_size*(header.size-i-1));
    header.size--;
    da_store_header(array,&header);
    return da_fake_pop(tp_size,array);
}

int da_resize(size_t tp_size,void *array,size_t size){
    da_generic_t header;
    da_load_header(array,&header);
    if(size==0){
        free(header.arr);
        memset(array,0,sizeof(da_generic_t));
        return 0;
    }
    void *p=realloc(header.arr,size*tp_size);
    if(!p){
        da_log_err("da_resize: realloc %zu bytes failed: %s\n",size*tp_size,strerror(errno));
        return 127;
    }
    header.arr=p;
    header.real=size;
    if(header.real<header.size){
        header.size=header.real;
    }
    da_store_header(array,&header);
    return 0;
}

int da_fake_pop(size_t tp_size,void *array){
    da_generic_t header;
    da_load_header(array,&header);
    if(header.size<header.real/2){
        void *p=realloc(header.arr,header.real/2*tp_size);
        if(p){
            header.arr=p;
            header.real/=2;
            da_store_header(array,&header);
        }else{
            da_log_err("da_fake_pop: realloc %zu bytes failed: %s\n",header.real/2*tp_size,strerror(errno));
        }
    }
    return 0;
}

int da_pop(size_t tp_size,void *array){
    da_generic_t header;
    da_load_header(array,&header);
    if(!header.size){
        return 0;
    }
    header.size--;
    da_store_header(array,&header);
    return da_fake_pop(tp_size,array);
}

int da_fake_clear(void *array){
    da_generic_t header;
    da_load_header(array,&header);
    header.size=0;
    da_store_header(array,&header);
    return 0;
}

int da_clear(void *array){
    da_generic_t header;
    da_load_header(array,&header);
    free(header.arr);
    memset(array,0,sizeof(da_generic_t));
    return 0;
}

#undef da_log_err

#endif

#endif