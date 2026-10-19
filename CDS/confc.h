#ifndef CONFC_H
#define CONFC_H

#if !(defined (__GNUG__)&&defined(size_t))
typedef __SIZE_TYPE__ size_t;
#ifdef __BEOS__
typedef long ssize_t;
#endif
#endif

#include <stdio.h>


/**
 * @brief 在以 ':' 分隔的 path 中查找可执行的 target。
 *
 * 每个路径项支持 ~、~用户名、$VAR_NAME 和 ${VAR} 展开；命中条件是普通文件
 * 且对当前用户可执行。
 *
 * @param[in] path   ':' 分隔的目录列表
 * @param[in] target 要查找的可执行文件名；含 '/' 时按路径本身判断，不搜索 path
 * @return malloc 出来的完整路径，由调用者 free；找不到返回 NULL
 */
char *confc_find_path(const char *path,const char *target);

/**
 * @brief 同 confc_find_path，另外报告命中的是 path 里的哪一项。
 *
 * @param[in]  path   ':' 分隔的目录列表
 * @param[in]  target 要查找的可执行文件名
 * @param[out] start  命中时指向 path 中那个路径项的起点；未命中、target 本身
 *                    是绝对路径时为 NULL；可以传 NULL
 * @return malloc 出来的完整路径，由调用者 free；找不到返回 NULL
 */
char *confc_ex_find_path(const char *path,const char *target,char **start);

/**
 * @brief 解析转义序列，返回转义后的字符。
 *
 * str 指向引导反斜杠之后的文本。支持下面这些写法：
 * \\a \\b \\f \\n \\r \\t \\v \\0 \\\\ \\' \\" \\?，
 * 以及数字转义 \\xHH（1~2 位十六进制）和 \\NNN（1~3 位八进制）；
 * 未知转义原样返回该字符。
 *
 * @param[in] str 指向反斜杠之后的文本（不含那个反斜杠）
 * @return 转义后的字符；str 为 NULL 返回 0，str 为空（末尾孤立反斜杠）时
 *         返回反斜杠本身
 */
char confc_parse_esc(const char *str);

/**
 * @brief 同 confc_parse_esc，并报告消费了几个字符。
 *
 * @param[in]  str 指向反斜杠之后的文本
 * @param[out] n   从 str 消费的字符数（0~3）；可以传 NULL
 * @return 转义后的字符
 */
char confc_ex_parse_esc(const char *str,size_t *n);

/**
 * @brief 解析 \\uXXXX / \\UXXXXXXXX 形式的 Unicode 转义，输出 UTF-8。
 *
 * @param[in] str 指向反斜杠之后的文本，例如 "u4F60"
 * @return malloc 出来的 UTF-8 字符串（1~4 字节 + 结尾 0），由调用者 free；
 *         格式不对、落在代理区或超过 U+10FFFF 时返回 NULL
 */
char *confc_parse_esc_unicode(const char *str);

/**
 * @brief 同 confc_parse_esc_unicode，并报告消费了几个字符。
 *
 * @param[in]  str 指向反斜杠之后的文本
 * @param[out] n   消费的字符数（\\uXXXX 为 5，\\UXXXXXXXX 为 9），失败时为 0；
 *                 可以传 NULL
 * @return malloc 出来的 UTF-8 字符串，由调用者 free；失败返回 NULL
 */
char *confc_ex_parse_esc_unicode(const char *str,size_t *n);

/**
 * @brief 解析以反斜杠开头的完整转义，返回还原出的字符串。
 *
 * @param[in] str 指向反斜杠的文本
 * @return malloc 出来的字符串，由调用者 free；str 不以反斜杠开头或后面为空时
 *         返回 NULL
 */
char *confc_parse_esc_all(const char *str);

/**
 * @brief 同 confc_parse_esc_all，并报告消费了几个字符。
 *
 * @param[in]  str 指向反斜杠的文本
 * @param[out] n   消费的字符数（含开头那个反斜杠）；可以传 NULL
 * @return malloc 出来的字符串，由调用者 free；失败返回 NULL
 */
char *confc_ex_parse_esc_all(const char *str,size_t *n);

/**
 * @brief 把字符串转成 long，按 C 规则（strict=0）。
 *
 * @param[in] str 输入字符串
 * @return 解析出的整数；解析失败返回 0
 */
long confc_str_to_int(const char *str);

/**
 * @brief 把字符串转成 long。
 *
 * strict=0 按 C 规则：可选正负号、0x 十六进制、前导 0 八进制、十进制，末尾
 * 可以跟 u/l/ul/ull 等后缀，数字里不允许下划线。
 * strict=1 按 TOML 规则：0x/0o/0b/十进制，数字之间允许下划线，没有 C 后缀。
 *
 * @param[in]  str    输入字符串
 * @param[out] n      消费的字符数；可以传 NULL
 * @param[out] error  0 正常；1 解析完还有剩余字符；2 没能解析出完整值
 *                    （溢出、下划线位置不合法也归到这一类）；可以传 NULL
 * @param[in]  strict 非 0 表示启用 TOML 规则
 * @return 解析出的整数（error 为 2 时返回已经解析到的部分）
 */
long confc_ex_str_to_int(const char *str,size_t *n,int *error,int strict);

/**
 * @brief 把字符串转成 double，按 C 规则（strict=0）。
 *
 * @param[in] str 输入字符串
 * @return 解析出的浮点数；解析失败返回 0.0
 */
double confc_str_to_float(const char *str);

/**
 * @brief 把字符串转成 double。
 *
 * strict=0 按 C 规则：strtod 的写法（含 inf/nan、十六进制浮点），末尾可以跟
 * f/F/l/L。strict=1 按 TOML 规则：允许 inf/nan，数字之间允许下划线，没有后缀、
 * 没有十六进制浮点。
 *
 * @param[in]  str    输入字符串
 * @param[out] n      消费的字符数；可以传 NULL
 * @param[out] error  0 正常；1 解析完还有剩余字符；2 没能解析出完整值；可以传 NULL
 * @param[in]  strict 非 0 表示启用 TOML 规则
 * @return 解析出的浮点数（error 为 2 时返回已经解析到的部分）
 */
double confc_ex_str_to_float(const char *str,size_t *n,int *error,int strict);

/**
 * @brief 把字符串转成 Unix 时间戳（秒）。
 *
 * 支持 TOML 写法：YYYY-MM-DD、YYYY-MM-DD[Tt ]HH:MM:SS、可带小数秒，
 * 以及可选的 Z/z 或 ±HH:MM 时区；没有时区时按 UTC 处理。
 *
 * @param[in] str 输入字符串
 * @return Unix 时间戳（秒）；解析失败返回 0
 */
unsigned long confc_str_to_date(const char *str);

/**
 * @brief 把字符串转成时间戳，并给出相对 UTC 的偏移。
 *
 * @param[in]  str        输入字符串
 * @param[out] time_zone  时区偏移（秒，东为正），Z 或没写时区时为 0；
 *                        可以传 NULL
 * @param[out] n          消费的字符数；可以传 NULL
 * @param[out] error      0 正常；1 解析完还有剩余字符；2 没能解析出完整值
 *                        （日期越界、字段非法也归到这一类）；可以传 NULL
 * @return Unix 时间戳（秒）
 */
unsigned long confc_ex_str_to_date(const char *str,long *time_zone,size_t *n,int *error);



typedef struct {
    char *key;
    char *value;
}ini_pair_t;

typedef struct {
    char *name;
    size_t pair_size;
    ini_pair_t *pairs;
}ini_section_t;

typedef struct {
    size_t section_size;
    ini_section_t *sections;
}ini_t;

ini_t ini_load(const char *filename);
ini_t ini_load_from_memory(const char *file);
void ini_unload(ini_t *file);

void ini_write(FILE *fp,ini_t *file);
void ini_write_fd(int fd,ini_t *file);
char *ini_write_to_memory(ini_t *file);

ini_section_t *ini_add_section(ini_t *file,ini_section_t *section);
void ini_add_pair(ini_section_t *section,ini_pair_t *pair);

int ini_del_section(ini_t *file,const char *section);
int ini_del_pair(ini_t *file,const char *section,const char *key);
int ini_del_pair_from_section(ini_section_t *section,const char *key);

const char *ini_lookup(ini_t *file,const char *section,const char *key);
ini_section_t *ini_lookup_section(ini_t *file,const char *name);
const char *ini_lookup_from_section(ini_section_t *section,const char *key);


typedef enum {
    TOML_STRING,
    TOML_INTEGER,
    TOML_FLOAT,
    TOML_BOOLEAN,
    TOML_DATETIME,
    TOML_ARRAY,
    TOML_TABLE,
    TOML_ARRAY_OF_TABLES
}toml_type_t;

typedef struct {
    char *key;
    toml_type_t *value;
}toml_pair_t;

typedef struct {
    toml_type_t tp;
    char *str;
}toml_string_t;

typedef struct {
    toml_type_t tp;
    long v;
}toml_integer_t;

typedef struct {
    toml_type_t tp;
    double v;
}toml_float_t;

typedef struct {
    toml_type_t tp;
    int v;
}toml_boolean_t;

typedef struct {
    toml_type_t tp;
    unsigned long time;
    long offset;
}toml_datetime_t;

typedef struct {
    toml_type_t tp;
    toml_type_t **items;
    size_t size;
}toml_array_t;

typedef struct {
    toml_type_t tp;
    const char *name;
    toml_pair_t *pairs;
    size_t size;
}toml_table_t;

typedef struct {
    toml_type_t tp;
    const char *name;
    struct toml_table_item_t{
        toml_pair_t *pairs;
        size_t size;
    } *table;
    size_t size;
}toml_array_of_tables_t;

typedef struct {
    size_t size;
    toml_pair_t *pairs;
}toml_t;


toml_t toml_load(const char *filename);
toml_t toml_load_from_memory(const char *file);
void toml_unload(toml_t *file);

void toml_add_table(toml_t *file,toml_table_t *table);
void toml_add_pair(toml_table_t *table,toml_pair_t *pair);

toml_type_t *toml_lookup(toml_t *file,const char *table,size_t i,const char *key);
toml_type_t *toml_lookup_table(toml_t *file,const char *name);
toml_type_t *toml_lookup_from_table(toml_table_t *table,const char *key);
toml_type_t *toml_lookup_from_array_of_tables(toml_array_of_tables_t *table,size_t i,const char *key);



#ifdef CONFC_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <limits.h>
#include <math.h>
#include <sys/stat.h>

#ifdef CONFC_DEBUG_OUTPUT
#define confc_log_err(...) fprintf(stderr,__VA_ARGS__)
#else
#define confc_log_err(...) ((void)0)
#endif

#ifndef DARRAY_H

#define darray_t(Tp) \
struct{\
    Tp *arr;\
    size_t size,real;\
}

inline static int da_init(void *array);
inline static int da_add(size_t tp_size,void *array,const void *buf);
inline static int da_clear(void *array);

#endif

#ifndef DA_STR_T
#define DA_STR_T
typedef darray_t(char) da_str;
#endif



char *confc_find_path(const char *path,const char *target){
    return confc_ex_find_path(path,target,NULL);
}
char *confc_ex_find_path(const char *path,const char *target,char **start){
    if(start!=NULL){
        *start=NULL;
    }

    if(target==NULL||target[0]=='\0'){
        return NULL;
    }

    /* target 带 '/' 时本身就是一个路径，不搜索 path */
    if(target[0]=='/'){
        struct stat status;
        if(stat(target,&status)!=0||!S_ISREG(status.st_mode)||access(target,X_OK)!=0){
            return NULL;
        }
        size_t size=strlen(target)+1;
        char *copy=malloc(size);
        if(!copy){
            confc_log_err("confc_ex_find_path: malloc %zu bytes failed: %s\n",size,strerror(errno));
            return NULL;
        }
        memcpy(copy,target,size);
        return copy;
    }
    if(path==NULL){
        return NULL;
    }

    da_str dir;
    da_init(&dir);
    da_str candidate;
    da_init(&candidate);
    da_str name;
    da_init(&name);

    char *ret=NULL;
    const char *entry=path;
    int failed=0;

    while(ret==NULL&&!failed){
        const char *end=entry;
        while(*end!='\0'&&*end!=':'){
            end++;
        }

        /* 把当前路径项展开到 dir */
        const char *pos=entry;
        dir.size=0;

        if(pos<end&&*pos=='~'){
            const char *user=pos+1;
            const char *user_end=user;
            while(user_end<end&&*user_end!='/'){ user_end++; }

            const char *home=NULL;
            if(user_end==user){
                home=getenv("HOME");
                if(home==NULL||home[0]=='\0'){
                    struct passwd *pw=getpwuid(getuid());
                    home=pw?pw->pw_dir:NULL;
                }
            }else{
                name.size=0;
                for(const char *p=user;p<user_end;p++){
                    failed|=da_add(sizeof(char),&name,p);
                }
                failed|=da_add(sizeof(char),&name,"");
                if(!failed){
                    struct passwd *pw=getpwnam(name.arr);
                    home=pw?pw->pw_dir:NULL;
                }
            }

            if(home!=NULL&&home[0]!='\0'){
                for(const char *p=home;*p!='\0';p++){
                    failed|=da_add(sizeof(char),&dir,p);
                }
            }else{
                /* 取不到家目录（如用户名不存在）时原样保留 */
                failed|=da_add(sizeof(char),&dir,"~");
                for(const char *p=user;p<user_end;p++){
                    failed|=da_add(sizeof(char),&dir,p);
                }
            }
            pos=user_end;
        }

        while(pos<end){
            const char *var=NULL;
            size_t var_size=0;
            const char *next=pos;

            if(*pos=='$'&&pos+1<end){
                if(pos[1]=='{'){
                    const char *close=pos+2;
                    while(close<end&&*close!='}'){
                        close++;
                    }
                    if(close>pos+2&&close<end){
                        int ok=((pos[2]>='a'&&pos[2]<='z')||(pos[2]>='A'&&pos[2]<='Z')||pos[2]=='_');
                        for(const char *p=pos+3;ok&&p<close;p++){
                            char c=*p;
                            if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')){
                                ok=0;
                            }
                        }
                        if(ok){
                            var=pos+2;
                            var_size=close-pos-2;
                            next=close+1;
                        }
                    }
                }else{
                    char c=pos[1];
                    if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'){
                        const char *p=pos+2;
                        while(p<end){
                            c=*p;
                            if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')){
                                break;
                            }
                            p++;
                        }
                        var=pos+1;
                        var_size=p-pos-1;
                        next=p;
                    }
                }
            }

            if(var==NULL){
                /* 其他变量写法、单独的 '$' 一律原样保留 */
                failed|=da_add(sizeof(char),&dir,pos);
                pos++;
                continue;
            }

            name.size=0;
            for(const char *p=var;p<var+var_size;p++){
                failed|=da_add(sizeof(char),&name,p);
            }
            failed|=da_add(sizeof(char),&name,"");
            const char *value=failed?NULL:getenv(name.arr);
            if(value!=NULL){
                for(const char *p=value;*p!='\0';p++){
                    failed|=da_add(sizeof(char),&dir,p);
                }
            }
            pos=next;
        }

        if(dir.size==0){
            if(end!=entry){
                /* 展开后为空的路径项（如引用了未定义的变量）跳过 */
                if(*end=='\0'){
                    break;
                }
                entry=end+1;
                continue;
            }
            /* 空路径项表示当前目录 */
            failed|=da_add(sizeof(char),&dir,".");
        }

        if(!failed){
            candidate.size=0;
            for(size_t i=0;i<dir.size;i++){
                failed|=da_add(sizeof(char),&candidate,&dir.arr[i]);
            }
            if(dir.arr[dir.size-1]!='/'){
                failed|=da_add(sizeof(char),&candidate,"/");
            }
            for(const char *p=target;*p!='\0';p++){
                failed|=da_add(sizeof(char),&candidate,p);
            }
            failed|=da_add(sizeof(char),&candidate,"");

            if(!failed){
                struct stat status;
                if(stat(candidate.arr,&status)==0&&S_ISREG(status.st_mode)&&access(candidate.arr,X_OK)==0){
                    if(start!=NULL){
                        /* 命中的路径项在 path 里的起点（path 是 const，这里按声明转掉 const） */
                        *start=(char *)entry;
                    }
                    ret=candidate.arr;
                    candidate.arr=NULL;
                    candidate.size=0;
                    candidate.real=0;
                    break;
                }
            }
        }

        if(*end=='\0'){
            break;
        }
        entry=end+1;
    }

    da_clear(&dir);
    da_clear(&candidate);
    da_clear(&name);
    return ret;
}

/* 解析 '\' 后面的转义序列
   confc_ex_parse_esc:         返回转义后的单个字符；*n 返回从 str 消费了多少字符（0~3）
   confc_parse_esc:            同 confc_ex_parse_esc，不要消费长度
   confc_ex_parse_esc_unicode: 处理 \uXXXX / \UXXXXXXXX，返回 malloc 的 UTF-8 字符串
                               （调用者 free），*n 返回消费的字符数（5 或 9）；
                               格式不对或码点非法返回 NULL
   confc_parse_esc_unicode:    同 confc_ex_parse_esc_unicode，不要消费长度 */
char confc_parse_esc(const char *str){
    return confc_ex_parse_esc(str,NULL);
} 
char confc_ex_parse_esc(const char *str,size_t *n){
    if(n!=NULL){
        *n=0;
    }
    if(str==NULL){
        return '\0';
    }
    if(str[0]=='\0'){
        return '\\';
    }

    /* \xHH：'x' + 1~2 位十六进制 */
    if(str[0]=='x'){
        unsigned value=0;
        size_t count=0;
        for(size_t i=1;i<=2&&str[i]!='\0';i++){
            char c=str[i];
            unsigned d;
            if(c>='0'&&c<='9'){
                d=(unsigned)(c-'0');
            }else if(c>='a'&&c<='f'){
                d=(unsigned)(c-'a'+10);
            }else if(c>='A'&&c<='F'){
                d=(unsigned)(c-'A'+10);
            }else{
                break;
            }
            value=value*16+d;
            count++;
        }
        if(count==0){
            if(n!=NULL){
                *n=1;
            }
            return 'x';
        }
        if(n!=NULL){
            *n=1+count;
        }
        return (char)value;
    }

    /* \NNN：1~3 位八进制 */
    if(str[0]>='0'&&str[0]<='7'){
        unsigned value=0;
        size_t count=0;
        for(size_t i=0;i<3&&str[i]>='0'&&str[i]<='7';i++){
            value=value*8+(unsigned)(str[i]-'0');
            count++;
        }
        if(n!=NULL){
            *n=count;
        }
        return (char)value;
    }

    if(n!=NULL){
        *n=1;
    }
    switch(str[0]){
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        case '?': return '?';
        default: return str[0];
    }
}

char *confc_parse_esc_unicode(const char *str){
    return confc_ex_parse_esc_unicode(str,NULL);
}
char *confc_ex_parse_esc_unicode(const char *str,size_t *n){
    if(n!=NULL){
        *n=0;
    }
    if(str==NULL){
        return NULL;
    }

    size_t digits;
    if(str[0]=='u'){
        digits=4;
    }else if(str[0]=='U'){
        digits=8;
    }else{
        return NULL;
    }

    unsigned long code=0;
    for(size_t i=1;i<=digits;i++){
        char c=str[i];
        unsigned d;
        if(c>='0'&&c<='9'){
            d=(unsigned)(c-'0');
        }else if(c>='a'&&c<='f'){
            d=(unsigned)(c-'a'+10);
        }else if(c>='A'&&c<='F'){
            d=(unsigned)(c-'A'+10);
        }else{
            return NULL;
        }
        code=code*16+d;
    }

    if(code>0x10FFFF||(code>=0xD800&&code<=0xDFFF)){
        confc_log_err("confc_ex_parse_esc_unicode: invalid code point 0x%lX\n",code);
        return NULL;
    }

    char *ret=malloc(5);
    if(!ret){
        confc_log_err("confc_ex_parse_esc_unicode: malloc 5 bytes failed: %s\n",strerror(errno));
        return NULL;
    }

    size_t size=0;
    if(code<0x80){
        ret[size++]=(char)code;
    }else if(code<0x800){
        ret[size++]=(char)(0xC0|(code>>6));
        ret[size++]=(char)(0x80|(code&0x3F));
    }else if(code<0x10000){
        ret[size++]=(char)(0xE0|(code>>12));
        ret[size++]=(char)(0x80|((code>>6)&0x3F));
        ret[size++]=(char)(0x80|(code&0x3F));
    }else{
        ret[size++]=(char)(0xF0|(code>>18));
        ret[size++]=(char)(0x80|((code>>12)&0x3F));
        ret[size++]=(char)(0x80|((code>>6)&0x3F));
        ret[size++]=(char)(0x80|(code&0x3F));
    }
    ret[size]='\0';

    if(n!=NULL){
        *n=1+digits;
    }
    return ret;
}

char *confc_parse_esc_all(const char *str){
    return confc_ex_parse_esc_all(str,NULL);
}
char *confc_ex_parse_esc_all(const char *str,size_t *n){
    if(n!=NULL){
        *n=0;
    }
    if(str==NULL||str[0]!='\\'||str[1]=='\0'){
        return NULL;
    }

    if(str[1]=='u'||str[1]=='U'){
        size_t used=0;
        char *r=confc_ex_parse_esc_unicode(&str[1],&used);
        if(r!=NULL&&n!=NULL){
            *n=used+1;
        }
        return r;
    }

    char *r=malloc(2);
    if(r==NULL){
        confc_log_err("confc_ex_parse_esc_all: malloc 2 bytes failed: %s\n",strerror(errno));
        return NULL;
    }
    size_t used=0;
    r[0]=confc_ex_parse_esc(&str[1],&used);
    r[1]='\0';
    if(n!=NULL){
        *n=used+1;
    }
    return r;
}

/* 字符串 -> 整数
   strict=0：遵守 C 规则（0x 十六进制、前导 0 八进制、十进制，末尾可跟 u/l/ul/ull 等后缀）
   strict=1：遵守 TOML 规则（0x/0o/0b/十进制，数字之间允许 '_'，没有 C 后缀）
   *n 返回消费的字符数，*error：0 正常，1 解析完还有剩余字符，2 没能解析出完整值 */
long confc_str_to_int(const char *str){
    return confc_ex_str_to_int(str,NULL,NULL,0);
}
long confc_ex_str_to_int(const char *str,size_t *n,int *error,int strict){
    if(n!=NULL){
        *n=0;
    }
    if(error!=NULL){
        *error=2;
    }
    if(str==NULL){
        return 0;
    }

    long value=0;
    size_t used=0;
    int complete=0;

    if(!strict){
        errno=0;
        char *end=NULL;
        value=strtol(str,&end,0);
        if(end!=str){
            used=(size_t)(end-str);
            complete=1;

            /* 只认 u、l、ll、ul、lu、ull、llu 这些后缀（大小写均可） */
            size_t i=used;
            while(i-used<3&&(str[i]=='u'||str[i]=='U'||str[i]=='l'||str[i]=='L')){
                i++;
            }
            if(i>used){
                char up[4];
                size_t len=i-used;
                for(size_t k=0;k<len;k++){
                    char c=str[used+k];
                    up[k]=(char)((c>='a'&&c<='z')?(c-'a'+'A'):c);
                }
                up[len]='\0';
                if(strcmp(up,"U")==0||strcmp(up,"L")==0||strcmp(up,"LL")==0||
                   strcmp(up,"UL")==0||strcmp(up,"LU")==0||
                   strcmp(up,"ULL")==0||strcmp(up,"LLU")==0){
                    used=i;
                }
            }
            if(errno==ERANGE){
                complete=0;
            }
        }
    }else{
        const char *p=str;
        int neg=0;
        if(*p=='+'||*p=='-'){
            neg=(*p=='-');
            p++;
        }

        int base=10;
        if(p[0]=='0'&&(p[1]=='x'||p[1]=='X')){
            base=16;
            p+=2;
        }else if(p[0]=='0'&&(p[1]=='o'||p[1]=='O')){
            base=8;
            p+=2;
        }else if(p[0]=='0'&&(p[1]=='b'||p[1]=='B')){
            base=2;
            p+=2;
        }

        unsigned long acc=0;
        size_t digits=0;
        int overflow=0;
        int underscore=0;

        while(*p!='\0'){
            char c=*p;
            if(c=='_'){
                /* '_' 只能夹在数字之间 */
                if(digits==0||underscore){
                    break;
                }
                underscore=1;
                p++;
                continue;
            }
            int d=-1;
            if(c>='0'&&c<='9'){
                d=c-'0';
            }else if(c>='a'&&c<='f'){
                d=c-'a'+10;
            }else if(c>='A'&&c<='F'){
                d=c-'A'+10;
            }
            if(d<0||d>=base){
                break;
            }
            underscore=0;
            if(acc>(~0UL-(unsigned long)d)/(unsigned long)base){
                overflow=1;
            }
            acc=acc*(unsigned long)base+(unsigned long)d;
            digits++;
            p++;
        }

        if(digits>0&&!underscore){
            used=(size_t)(p-str);
            complete=1;
            if(neg){
                if(overflow||acc>((unsigned long)LONG_MAX)+1UL){
                    complete=0;
                    value=LONG_MIN;
                }else{
                    value=(long)(0-acc);
                }
            }else{
                if(overflow||acc>(unsigned long)LONG_MAX){
                    complete=0;
                    value=LONG_MAX;
                }else{
                    value=(long)acc;
                }
            }
        }
    }

    if(n!=NULL){
        *n=used;
    }
    if(error!=NULL){
        *error=complete?((str[used]!='\0')?1:0):2;
    }
    return value;
}

/* 字符串 -> 浮点数
   strict=0：遵守 C 规则（strtod，末尾可跟 f/F/l/L）
   strict=1：遵守 TOML 规则（允许 inf/nan，数字之间允许 '_'，没有后缀、没有十六进制浮点）
   *n 返回消费的字符数，*error 同 confc_ex_str_to_int */
double confc_str_to_float(const char *str){
    return confc_ex_str_to_float(str,NULL,NULL,0);
}
double confc_ex_str_to_float(const char *str,size_t *n,int *error,int strict){
    if(n!=NULL){
        *n=0;
    }
    if(error!=NULL){
        *error=2;
    }
    if(str==NULL){
        return 0.0;
    }

    double value=0.0;
    size_t used=0;
    int complete=0;

    if(!strict){
        errno=0;
        char *end=NULL;
        value=strtod(str,&end);
        if(end!=str){
            used=(size_t)(end-str);
            complete=1;
            if(str[used]=='f'||str[used]=='F'||str[used]=='l'||str[used]=='L'){
                used++;
            }
            if(errno==ERANGE){
                complete=0;
            }
        }
    }else{
        const char *p=str;
        int neg=0;
        int has_us=0;
        if(*p=='+'||*p=='-'){
            neg=(*p=='-');
            p++;
        }

        if(strncmp(p,"inf",3)==0){
            value=neg?-INFINITY:INFINITY;
            p+=3;
            complete=1;
        }else if(strncmp(p,"nan",3)==0){
            value=NAN;
            p+=3;
            complete=1;
        }else{
            int bad=0;

            /* 整数部分 */
            size_t digits=0;
            int underscore=0;
            while(*p!='\0'){
                char c=*p;
                if(c=='_'){
                    if(digits==0||underscore){
                        bad=1;
                        break;
                    }
                    underscore=1;
                    has_us=1;
                    p++;
                    continue;
                }
                if(c>='0'&&c<='9'){
                    underscore=0;
                    digits++;
                    p++;
                    continue;
                }
                break;
            }
            if(digits==0||underscore){
                bad=1;
            }

            /* 小数部分 */
            if(!bad&&*p=='.'){
                p++;
                size_t fdigits=0;
                underscore=0;
                while(*p!='\0'){
                    char c=*p;
                    if(c=='_'){
                        if(fdigits==0||underscore){
                            bad=1;
                            break;
                        }
                        underscore=1;
                        has_us=1;
                        p++;
                        continue;
                    }
                    if(c>='0'&&c<='9'){
                        underscore=0;
                        fdigits++;
                        p++;
                        continue;
                    }
                    break;
                }
                if(fdigits==0||underscore){
                    bad=1;
                }
            }

            /* 指数部分；不完整时把 'e' 留作尾随字符 */
            if(!bad&&(*p=='e'||*p=='E')){
                const char *ep=p;
                p++;
                if(*p=='+'||*p=='-'){
                    p++;
                }
                size_t edigits=0;
                int ebad=0;
                underscore=0;
                while(*p!='\0'){
                    char c=*p;
                    if(c=='_'){
                        if(edigits==0||underscore){
                            ebad=1;
                            break;
                        }
                        underscore=1;
                        has_us=1;
                        p++;
                        continue;
                    }
                    if(c>='0'&&c<='9'){
                        underscore=0;
                        edigits++;
                        p++;
                        continue;
                    }
                    break;
                }
                if(ebad||underscore||edigits==0){
                    p=ep;
                }
            }

            if(!bad){
                /* 语法检查完了，数值交给 strtod，避免自己累加带来的精度偏差 */
                complete=1;
                if(!has_us){
                    value=strtod(str,NULL);
                }else{
                    size_t len=(size_t)(p-str);
                    char *clean=malloc(len+1);
                    if(clean==NULL){
                        complete=0;
                    }else{
                        size_t k=0;
                        for(size_t j=0;j<len;j++){
                            if(str[j]!='_'){
                                clean[k++]=str[j];
                            }
                        }
                        clean[k]='\0';
                        value=strtod(clean,NULL);
                        free(clean);
                    }
                }
            }
        }

        if(complete){
            used=(size_t)(p-str);
        }
    }

    if(n!=NULL){
        *n=used;
    }
    if(error!=NULL){
        *error=complete?((str[used]!='\0')?1:0):2;
    }
    return value;
}

/* 字符串 -> 时间戳（Unix 秒）
   支持 TOML 写法：YYYY-MM-DD、YYYY-MM-DD[ Tt]HH:MM:SS、可带 .小数秒，
   以及可选的 Z 或 ±HH:MM 时区；没有时区按 UTC 处理。
   *time_zone 返回相对 UTC 的偏移（秒，东为正）；*n、*error 同前 */
unsigned long confc_str_to_date(const char *str){
    return confc_ex_str_to_date(str,NULL,NULL,NULL);
}
unsigned long confc_ex_str_to_date(const char *str,long *time_zone,size_t *n,int *error){
    if(n!=NULL){
        *n=0;
    }
    if(error!=NULL){
        *error=2;
    }
    if(time_zone!=NULL){
        *time_zone=0;
    }
    if(str==NULL){
        return 0;
    }

    const char *p=str;
    long year=0,month=0,day=0,hour=0,minute=0,second=0;
    long tz=0;
    size_t used=0;

    do{
        /* YYYY-MM-DD */
        if(p[0]<'0'||p[0]>'9'){ break; }
        if(p[1]<'0'||p[1]>'9'){ break; }
        if(p[2]<'0'||p[2]>'9'){ break; }
        if(p[3]<'0'||p[3]>'9'){ break; }
        year=(p[0]-'0')*1000+(p[1]-'0')*100+(p[2]-'0')*10+(p[3]-'0');
        if(p[4]!='-'){ break; }
        p+=5;

        if(p[0]<'0'||p[0]>'9'||p[1]<'0'||p[1]>'9'){ break; }
        month=(p[0]-'0')*10+(p[1]-'0');
        if(p[2]!='-'){ break; }
        p+=3;

        if(p[0]<'0'||p[0]>'9'||p[1]<'0'||p[1]>'9'){ break; }
        day=(p[0]-'0')*10+(p[1]-'0');
        p+=2;

        /* 可选的时间部分 */
        if(*p=='T'||*p=='t'||*p==' '){
            p++;

            if(p[0]<'0'||p[0]>'9'||p[1]<'0'||p[1]>'9'){ break; }
            hour=(p[0]-'0')*10+(p[1]-'0');
            if(p[2]!=':'){ break; }
            p+=3;

            if(p[0]<'0'||p[0]>'9'||p[1]<'0'||p[1]>'9'){ break; }
            minute=(p[0]-'0')*10+(p[1]-'0');
            if(p[2]!=':'){ break; }
            p+=3;

            if(p[0]<'0'||p[0]>'9'||p[1]<'0'||p[1]>'9'){ break; }
            second=(p[0]-'0')*10+(p[1]-'0');
            p+=2;

            /* 小数秒：只消费，不参与时间戳 */
            if(*p=='.'){
                p++;
                size_t fdigits=0;
                while(*p>='0'&&*p<='9'){
                    p++;
                    fdigits++;
                }
                if(fdigits==0){ break; }
            }

            /* 时区 */
            if(*p=='Z'||*p=='z'){
                p++;
                tz=0;
            }else if(*p=='+'||*p=='-'){
                long sign=(*p=='-')?-1:1;
                p++;
                if(p[0]<'0'||p[0]>'9'||p[1]<'0'||p[1]>'9'){ break; }
                long oh=(p[0]-'0')*10+(p[1]-'0');
                if(p[2]!=':'){ break; }
                if(p[3]<'0'||p[3]>'9'||p[4]<'0'||p[4]>'9'){ break; }
                long om=(p[3]-'0')*10+(p[4]-'0');
                if(oh>23||om>59){ break; }
                tz=sign*(oh*3600+om*60);
                p+=5;
            }
        }

        /* 范围检查 */
        if(month<1||month>12){ break; }
        static const int mdays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
        int leap=((year%4==0&&year%100!=0)||year%400==0);
        int dim=mdays[month-1]+((month==2&&leap)?1:0);
        if(day<1||day>dim){ break; }
        if(hour>23||minute>59||second>60){ break; }

        /* 公历 -> 1970-01-01 起的天数 */
        long y=year-((month<=2)?1:0);
        long era=(y>=0?y:y-399)/400;
        long yoe=y-era*400;
        long doy=(153*(month+((month>2)?-3:9))+2)/5+day-1;
        long doe=yoe*365+yoe/4-yoe/100+doy;
        long days=era*146097+doe-719468;

        long long stamp=(long long)days*86400LL+hour*3600LL+minute*60LL+second-tz;

        used=(size_t)(p-str);
        (void)0;
        if(time_zone!=NULL){
            *time_zone=tz;
        }
        if(n!=NULL){
            *n=used;
        }
        if(error!=NULL){
            *error=(*p!='\0')?1:0;
        }
        return (unsigned long)stamp;
    }while(0);

    return 0;
}



ini_t ini_load(const char *filename){
    ini_t ret={0};
    if(filename==NULL){
        return ret;
    }
    int fd=open(filename,O_RDONLY);
    if(fd==-1){
        return ret;
    }

    struct stat status;
    if(fstat(fd,&status)!=0){
        close(fd);
        return ret;
    }
    char *mem=malloc(status.st_size+1);
    if(!mem){
        confc_log_err("ini_load: malloc %zu bytes failed: %s\n",(size_t)(status.st_size+1),strerror(errno));
        close(fd);
        return ret;
    }
    ssize_t got=0;
    while(got<(ssize_t)status.st_size){
        ssize_t n=read(fd,mem+got,status.st_size-got);
        if(n<=0){
            if(n==-1&&errno==EINTR){
                continue;
            }
            break;
        }
        got+=n;
    }
    close(fd);

    mem[got]='\0';
    memset(mem+got+1,0,status.st_size-got);
    ret=ini_load_from_memory(mem);

    free(mem);
    return ret;
}


static void ini_parse_per_line(ini_t *ret,ini_section_t **now_section,const char *file,size_t *i){
#define i (*i)

    da_str key,value;
    da_init(&key);
    da_init(&value);

    while(file[i]==' '){
        i++;
    }
    if(file[i]=='\n'||file[i]=='\r'||file[i]=='\0'){
        return ;
    }

    if(file[i]=='['){
        i++;
        while(file[i]==' '){
            i++;
        }
        while(file[i]!=']'&&file[i]!='\r'&&file[i]!='\0'){
            da_add(sizeof(char),&key,&file[i]);
            i++;
        }
        if(file[i]=='\r'||file[i]=='\0'){
            while(file[i]!='\n'&&file[i]!='\0'){
                i++;
            }
            da_clear(&key);
            return ;
        }
        while(key.size&&key.arr[key.size-1]==' '){
            key.size--;
        }

        da_add(sizeof(char),&key,"");
        ini_section_t section={.name=key.arr};
        *now_section=ini_add_section(ret,&section);
        da_clear(&key);
        while(file[i]!='\n'&&file[i]!='\0'){
            i++;
        }
        return ;
    }else if(file[i]=='#'){
        while(file[i]!='\n'&&file[i]!='\0'){
            i++;
        }
        return ;
    }

    while(file[i]!='='&&file[i]!='#'&&file[i]!='\r'&&file[i]!='\n'&&file[i]!='\0'){
        da_add(sizeof(char),&key,&file[i]);
        i++;
    }
    if(key.size==0){
        /* 空键（例如 "=value"）：整行丢弃 */
        while(file[i]!='\n'&&file[i]!='\0'){
            i++;
        }
        da_clear(&key);
        return ;
    }
    while(key.arr[key.size-1]==' '){
        key.size--;
    }
    da_add(sizeof(char),&key,"");

    if(file[i]!='='){
        goto L_ADD_PAIR;
    }

    i++;
    while(file[i]==' '){
        i++;
    }
    while(file[i]!='#'&&file[i]!='\r'&&file[i]!='\n'&&file[i]!='\0'){
        da_add(sizeof(char),&value,&file[i]);
        i++;
    }
    while(value.size&&value.arr[value.size-1]==' '){
        value.size--;
    }
    L_ADD_PAIR:
    da_add(sizeof(char),&value,"");

    ini_pair_t pair={.key=key.arr,.value=value.arr};
    if(*now_section==NULL){
        /* 根层的键放在一个名字为 "" 的 section 里；名字由 ini_add_section 复制 */
        ini_section_t section={.name=""};
        *now_section=ini_add_section(ret,&section);
    }
    ini_add_pair(*now_section,&pair);
    da_clear(&key);
    da_clear(&value);

    if(file[i]=='#'){
        while(file[i]!='\n'&&file[i]!='\0'){
            i++;
        }
    }

#undef i
}
ini_t ini_load_from_memory(const char *file){
    ini_t ret={0};
    if(file==NULL){
        return ret;
    }
    ini_section_t *now_section=NULL;

    size_t i=0;
    while(file[i]!='\0'){
        ini_parse_per_line(&ret,&now_section,file,&i);
        if(file[i]!='\0'){
            i++;
        }
    }

    return ret;
}

void ini_unload(ini_t *file){
    if(file==NULL){
        return ;
    }
    for(size_t i=0;i<file->section_size;i++){
        free(file->sections[i].name);
        for(size_t j=0;j<file->sections[i].pair_size;j++){
            free(file->sections[i].pairs[j].key);
            free(file->sections[i].pairs[j].value);
        }
        free(file->sections[i].pairs);
    }
    free(file->sections);
    file->sections=NULL;
    file->section_size=0;
}

char *ini_write_to_memory(ini_t *file){
    if(file==NULL){
        confc_log_err("ini_write_to_memory: file is nullptr\n");
        return NULL;
    }

    da_str mem;
    da_init(&mem);

    int failed=0;
    
    for(size_t i=0;i<file->section_size;i++){
        ini_section_t *section=&file->sections[i];
        if(section->name!=NULL&&section->name[0]!='\0'){
            failed|=da_add(sizeof(char),&mem,"[");
            for(const char *p=section->name;*p!='\0';p++){
                failed|=da_add(sizeof(char),&mem,p);
            }
            failed|=da_add(sizeof(char),&mem,"]");
            failed|=da_add(sizeof(char),&mem,"\n");
        }
        for(size_t j=0;j<section->pair_size;j++){
            ini_pair_t *pair=&section->pairs[j];
            if(pair->key!=NULL){
                for(const char *p=pair->key;*p!='\0';p++){
                    failed|=da_add(sizeof(char),&mem,p);
                }
            }
            failed|=da_add(sizeof(char),&mem,"=");
            if(pair->value!=NULL){
                for(const char *p=pair->value;*p!='\0';p++){
                    failed|=da_add(sizeof(char),&mem,p);
                }
            }
            failed|=da_add(sizeof(char),&mem,"\n");
        }
    }
    
    failed|=da_add(sizeof(char),&mem,"");

    if(failed){
        confc_log_err("ini_write_to_memory: out of memory\n");
        da_clear(&mem);
        return NULL;
    }
    return mem.arr;
}

void ini_write(FILE *fp,ini_t *file){
    if(fp==NULL||file==NULL){
        confc_log_err("ini_write: nullptr argument\n");
        return;
    }

    for(size_t i=0;i<file->section_size;i++){
        ini_section_t *section=&file->sections[i];
        if(section->name!=NULL&&section->name[0]!='\0'){
            fputc('[',fp);
            fputs(section->name,fp);
            fputc(']',fp);
            fputc('\n',fp);
        }
        for(size_t j=0;j<section->pair_size;j++){
            ini_pair_t *pair=&section->pairs[j];
            if(pair->key!=NULL){
                fputs(pair->key,fp);
            }
            fputc('=',fp);
            if(pair->value!=NULL){
                fputs(pair->value,fp);
            }
            fputc('\n',fp);
        }
    }
}

void ini_write_fd(int fd,ini_t *file){
    if(file==NULL){
        confc_log_err("ini_write_fd: file is nullptr\n");
        return;
    }

    for(size_t i=0;i<file->section_size;i++){
        ini_section_t *section=&file->sections[i];

        if(section->name!=NULL&&section->name[0]!='\0'){
            const char *piece[3]={"[",section->name,"]\n"};
            size_t size[3]={1,strlen(section->name),2};

            for(size_t k=0,off=0;k<3;){
                if(off==size[k]){
                    k++;
                    off=0;
                    continue;
                }
                ssize_t got=write(fd,piece[k]+off,size[k]-off);
                if(got<=0){
                    if(got==-1&&errno==EINTR){
                        continue;
                    }
                    return;
                }
                off+=got;
            }
        }

        for(size_t j=0;j<section->pair_size;j++){
            ini_pair_t *pair=&section->pairs[j];
            const char *piece[4]={pair->key?pair->key:"","=",pair->value?pair->value:"","\n"};
            size_t size[4]={strlen(piece[0]),1,strlen(piece[2]),1};

            for(size_t k=0,off=0;k<4;){
                if(off==size[k]){
                    k++;
                    off=0;
                    continue;
                }
                ssize_t got=write(fd,piece[k]+off,size[k]-off);
                if(got<=0){
                    if(got==-1&&errno==EINTR){
                        continue;
                    }
                    return;
                }
                off+=got;
            }
        }
    }
}

ini_section_t *ini_add_section(ini_t *file,ini_section_t *section){
    if(file==NULL||section==NULL){
        confc_log_err("ini_add_section: nullptr argument\n");
        return NULL;
    }

    const char *src_name=section->name?section->name:"";
    char *name=malloc(strlen(src_name)+1);
    if(!name){
        confc_log_err("ini_add_section: malloc %zu bytes failed: %s\n",(size_t)(strlen(src_name)+1),strerror(errno));
        return NULL;
    }
    strcpy(name,src_name);
    ini_section_t sec=*section;
    sec.name=name;

    if(file->section_size==0){
        file->sections=malloc(sizeof(ini_section_t));
        if(!file->sections){
            confc_log_err("ini_add_section: malloc %zu bytes failed: %s\n",sizeof(ini_section_t),strerror(errno));
            free(name);
            return NULL;
        }
        memcpy(&file->sections[0],&sec,sizeof(ini_section_t));
        file->section_size++;
        return &file->sections[0];
    }

    size_t l=0,r=file->section_size-1;
    size_t mid=l+(r-l)/2;
    while(l<=r&&r!=(size_t)-1){
        mid=l+(r-l)/2;
        int res=strcmp(file->sections[mid].name,sec.name);
        if(res==0){
            for(size_t i=0;i<sec.pair_size;i++){
                ini_add_pair(&file->sections[mid],&sec.pairs[i]);
            }
            free(name);
            return &file->sections[mid];
        }else if(res<0){
            l=mid+1;
        }else{
            r=mid-1;
        }
    }
    ini_section_t *sections=realloc(file->sections,(file->section_size+1)*sizeof(ini_section_t));
    if(!sections){
        confc_log_err("ini_add_section: realloc %zu bytes failed: %s\n",(size_t)((file->section_size+1)*sizeof(ini_section_t)),strerror(errno));
        free(name);
        return NULL;
    }
    file->sections=sections;
    memmove(&file->sections[l+1],&file->sections[l],(file->section_size-l)*sizeof(ini_section_t));
    memcpy(&file->sections[l],&sec,sizeof(ini_section_t));
    file->section_size++;

    return &file->sections[l];
}

void ini_add_pair(ini_section_t *section,ini_pair_t *pair){
    if(section==NULL||pair==NULL){
        confc_log_err("ini_add_pair: nullptr argument\n");
        return ;
    }

    const char *src_key=pair->key?pair->key:"";
    const char *src_value=pair->value?pair->value:"";
    ini_pair_t p=*pair;
    p.key=malloc(strlen(src_key)+1);
    if(!p.key){
        confc_log_err("ini_add_pair: malloc %zu bytes failed: %s\n",(size_t)(strlen(src_key)+1),strerror(errno));
        return ;
    }
    strcpy(p.key,src_key);
    p.value=malloc(strlen(src_value)+1);
    if(!p.value){
        confc_log_err("ini_add_pair: malloc %zu bytes failed: %s\n",(size_t)(strlen(src_value)+1),strerror(errno));
        free(p.key);
        return ;
    }
    strcpy(p.value,src_value);

    if(section->pair_size==0){
        section->pairs=malloc(sizeof(ini_pair_t));
        if(!section->pairs){
            confc_log_err("ini_add_pair: malloc %zu bytes failed: %s\n",sizeof(ini_pair_t),strerror(errno));
            free(p.key);
            free(p.value);
            return ;
        }
        memcpy(&section->pairs[0],&p,sizeof(ini_pair_t));
        section->pair_size++;
        return ;
    }

    size_t l=0,r=section->pair_size-1;
    size_t mid=l+(r-l)/2;
    while(l<=r&&r!=(size_t)-1){
        mid=l+(r-l)/2;
        int res=strcmp(section->pairs[mid].key,p.key);
        if(res==0){
            free(section->pairs[mid].key);
            free(section->pairs[mid].value);
            memcpy(&section->pairs[mid],&p,sizeof(ini_pair_t));
            return ;
        }else if(res<0){
            l=mid+1;
        }else{
            r=mid-1;
        }
    }
    ini_pair_t *pairs=realloc(section->pairs,(section->pair_size+1)*sizeof(ini_pair_t));
    if(!pairs){
        confc_log_err("ini_add_pair: realloc %zu bytes failed: %s\n",(size_t)((section->pair_size+1)*sizeof(ini_pair_t)),strerror(errno));
        free(p.key);
        free(p.value);
        return ;
    }
    section->pairs=pairs;
    memmove(&section->pairs[l+1],&section->pairs[l],(section->pair_size-l)*sizeof(ini_pair_t));
    memcpy(&section->pairs[l],&p,sizeof(ini_pair_t));
    section->pair_size++;
}

int ini_del_section(ini_t *file,const char *section){
    if(file==NULL||section==NULL){
        return 1;
    }
    if(file->section_size==0){
        return 1;
    }

    size_t l=0,r=file->section_size-1;
    size_t mid=l+(r-l)/2;
    while(l<=r&&r!=(size_t)-1){
        mid=l+(r-l)/2;
        int res=strcmp(file->sections[mid].name,section);
        if(res==0){
            break;
        }else if(res<0){
            l=mid+1;
        }else{
            r=mid-1;
        }
    }
    if(l>r||r==(size_t)-1){
        return 1;
    }

    ini_section_t *now=&file->sections[mid];
    for(size_t i=0;i<now->pair_size;i++){
        free(now->pairs[i].key);
        free(now->pairs[i].value);
    }
    free(now->pairs);
    free(now->name);

    memmove(&file->sections[mid],&file->sections[mid+1],(file->section_size-mid-1)*sizeof(ini_section_t));
    file->section_size--;
    ini_section_t *sections=realloc(file->sections,file->section_size*sizeof(ini_section_t));
    if(!sections&&file->section_size){
        confc_log_err("ini_del_section: realloc %zu bytes failed: %s\n",(size_t)(file->section_size*sizeof(ini_section_t)),strerror(errno));
    }else{
        file->sections=sections;
    }

    return 0;
}

int ini_del_pair(ini_t *file,const char *section,const char *key){
    if(file==NULL||section==NULL||key==NULL){
        return 1;
    }
    ini_section_t *sec=ini_lookup_section(file,section);
    if(sec==NULL){
        return 1;
    }
    return ini_del_pair_from_section(sec,key);
}

int ini_del_pair_from_section(ini_section_t *section,const char *key){
    if(section==NULL||key==NULL){
        return 1;
    }
    if(section->pair_size==0){
        return 2;
    }

    size_t l=0,r=section->pair_size-1;
    size_t mid=l+(r-l)/2;
    while(l<=r&&r!=(size_t)-1){
        mid=l+(r-l)/2;
        int res=strcmp(section->pairs[mid].key,key);
        if(res==0){
            break;
        }else if(res<0){
            l=mid+1;
        }else{
            r=mid-1;
        }
    }
    if(l>r||r==(size_t)-1){
        return 2;
    }

    free(section->pairs[mid].key);
    free(section->pairs[mid].value);

    memmove(&section->pairs[mid],&section->pairs[mid+1],(section->pair_size-mid-1)*sizeof(ini_pair_t));
    section->pair_size--;
    ini_pair_t *pairs=realloc(section->pairs,section->pair_size*sizeof(ini_pair_t));
    if(!pairs&&section->pair_size){
        confc_log_err("ini_del_pair_from_section: realloc %zu bytes failed: %s\n",(size_t)(section->pair_size*sizeof(ini_pair_t)),strerror(errno));
    }else{
        section->pairs=pairs;
    }

    return 0;
}

const char *ini_lookup(ini_t *file,const char *section,const char *key){
    ini_section_t *sec=ini_lookup_section(file,section);
    if(sec==NULL){
        return NULL;
    }
    return ini_lookup_from_section(sec,key);
}

ini_section_t *ini_lookup_section(ini_t *file,const char *name){
    if(file==NULL||name==NULL||file->section_size==0){
        return NULL;
    }

    size_t l=0,r=file->section_size-1;
    while(l<=r&&r!=(size_t)-1){
        size_t mid=l+(r-l)/2;
        int res=strcmp(file->sections[mid].name,name);
        if(res==0){
            return &file->sections[mid];
        }else if(res<0){
            l=mid+1;
        }else{
            r=mid-1;
        }
    }
    return NULL;
}

const char *ini_lookup_from_section(ini_section_t *section,const char *key){
    if(section==NULL||key==NULL||section->pair_size==0){
        return NULL;
    }

    size_t l=0,r=section->pair_size-1;
    size_t mid=l+(r-l)/2;
    while(l<=r&&r!=(size_t)-1){
        mid=l+(r-l)/2;
        int res=strcmp(section->pairs[mid].key,key);
        if(res==0){
            return section->pairs[mid].value;
        }else if(res<0){
            l=mid+1;
        }else{
            r=mid-1;
        }
    }
    return NULL;
}




toml_t toml_load(const char *filename){
    toml_t ret={0};
    if(filename==NULL){
        return ret;
    }
    int fd=open(filename,O_RDONLY);
    if(fd==-1){
        return ret;
    }

    struct stat status;
    if(fstat(fd,&status)!=0){
        close(fd);
        return ret;
    }
    char *mem=malloc(status.st_size+1);
    if(!mem){
        confc_log_err("toml_load: malloc %zu bytes failed: %s\n",(size_t)(status.st_size+1),strerror(errno));
        close(fd);
        return ret;
    }
    ssize_t got=0;
    while(got<(ssize_t)status.st_size){
        ssize_t n=read(fd,mem+got,status.st_size-got);
        if(n<=0){
            if(n==-1&&errno==EINTR){
                continue;
            }
            break;
        }
        got+=n;
    }
    close(fd);

    mem[got]='\0';
    memset(mem+got+1,0,status.st_size-got);
    ret=toml_load_from_memory(mem);

    free(mem);
    return ret;
}

static void toml_free_value(toml_type_t *value){
    if(value==NULL){
        return ;
    }

    toml_type_t tp=*value;
    if(tp==TOML_STRING){
        free(((toml_string_t *)value)->str);
    }else if(tp==TOML_ARRAY){
        toml_array_t *arr=(toml_array_t *)value;
        for(size_t i=0;i<arr->size;i++){
            toml_free_value(arr->items[i]);
        }
        free(arr->items);
    }else if(tp==TOML_TABLE){
        toml_table_t *table=(toml_table_t *)value;
        for(size_t i=0;i<table->size;i++){
            free(table->pairs[i].key);
            toml_free_value(table->pairs[i].value);
        }
        free(table->pairs);
        /* name 指向根 pairs 的 key 或调用方自己的字符串，这里不单独释放 */
    }else if(tp==TOML_ARRAY_OF_TABLES){
        toml_array_of_tables_t *aot=(toml_array_of_tables_t *)value;
        for(size_t i=0;i<aot->size;i++){
            for(size_t j=0;j<aot->table[i].size;j++){
                free(aot->table[i].pairs[j].key);
                toml_free_value(aot->table[i].pairs[j].value);
            }
            free(aot->table[i].pairs);
        }
        free(aot->table);
    }
    free(value);
}

/* 解析一个字符串字面量（基本/字面量/多行），返回 toml_string_t，失败返回 NULL */
static toml_type_t *toml_parse_string(const char *file,size_t *i){
#define i (*i)

    if(file[i]!='\"'&&file[i]!='\''){
        return NULL;
    }

    da_str value;
    da_init(&value);

    char q=file[i];
    int literal=(q=='\'');
    int multiline=0;

    if(file[i]==q&&file[i+1]==q&&file[i+2]==q&&file[i]!='\0'){
        multiline=1;
        i+=3;
        /* 开头的换行不算内容 */
        if(file[i]=='\r'){
            i++;
        }
        if(file[i]=='\n'){
            i++;
        }
    }else{
        i++;
    }

    while(file[i]!='\0'){
        if(multiline){
            if(file[i]==q&&file[i+1]==q&&file[i+2]==q){
                i+=3;
                break;
            }
        }else if(file[i]==q){
            i++;
            break;
        }

        if(!literal&&file[i]=='\\'){
            size_t n=0;
            char *r=confc_ex_parse_esc_all(&file[i],&n);
            if(r==NULL){
                i++;
                continue;
            }
            for(size_t k=0;r[k]!='\0';k++){
                da_add(sizeof(char),&value,&r[k]);
            }
            free(r);
            i+=n;
            continue;
        }

        /* 单行字符串里不允许裸换行 */
        if(!multiline&&(file[i]=='\n'||file[i]=='\r')){
            break;
        }

        da_add(sizeof(char),&value,&file[i]);
        i++;
    }

    da_add(sizeof(char),&value,"");
    if(value.arr==NULL){
        confc_log_err("toml_parse_string: out of memory\n");
        return NULL;
    }

    toml_string_t *str=malloc(sizeof(toml_string_t));
    if(str==NULL){
        confc_log_err("toml_parse_string: malloc %zu bytes failed: %s\n",sizeof(toml_string_t),strerror(errno));
        da_clear(&value);
        return NULL;
    }
    str->tp=TOML_STRING;
    str->str=value.arr;

    return (toml_type_t *)str;

#undef i
}

/* 值解析到这儿就算结束了：后面只剩空白、注释、行尾或者数组的分隔符 */
#define toml_value_end(c) ((c)=='\0'||(c)=='\n'||(c)=='\r'||(c)==' '||(c)=='\t'||(c)=='#'||(c)==','||(c)==']')

static void toml_parse_array(toml_array_t *arr,const char *file,size_t *i);

/* 解析一个值：字符串、数组、布尔、日期、整数、浮点 */
static toml_type_t *toml_parse_value(const char *file,size_t *i){
#define i (*i)

    /* 字符串 */
    if(file[i]=='\"'||file[i]=='\''){
        return toml_parse_string(file,&i);
    }

    /* 数组 */
    if(file[i]=='['){
        toml_array_t *arr=malloc(sizeof(toml_array_t));
        if(arr==NULL){
            confc_log_err("toml_parse_value: malloc %zu bytes failed: %s\n",sizeof(toml_array_t),strerror(errno));
            return NULL;
        }
        arr->tp=TOML_ARRAY;
        arr->items=NULL;
        arr->size=0;

        toml_parse_array(arr,file,&i);
        return (toml_type_t *)arr;
    }

    /* 布尔 */
    if(strncmp(&file[i],"true",4)==0&&toml_value_end(file[i+4])){
        toml_boolean_t *b=malloc(sizeof(toml_boolean_t));
        if(b==NULL){
            confc_log_err("toml_parse_value: malloc %zu bytes failed: %s\n",sizeof(toml_boolean_t),strerror(errno));
            return NULL;
        }
        b->tp=TOML_BOOLEAN;
        b->v=1;
        i+=4;
        return (toml_type_t *)b;
    }
    if(strncmp(&file[i],"false",5)==0&&toml_value_end(file[i+5])){
        toml_boolean_t *b=malloc(sizeof(toml_boolean_t));
        if(b==NULL){
            confc_log_err("toml_parse_value: malloc %zu bytes failed: %s\n",sizeof(toml_boolean_t),strerror(errno));
            return NULL;
        }
        b->tp=TOML_BOOLEAN;
        b->v=0;
        i+=5;
        return (toml_type_t *)b;
    }

    /* 日期是 YYYY-...，本地时间（07:32:00）暂时不支持；这两种都不当数字解析 */
    int date_like=0;
    if(file[i]>='0'&&file[i]<='9'&&file[i+1]>='0'&&file[i+1]<='9'&&file[i+2]>='0'&&file[i+2]<='9'&&file[i+3]>='0'&&file[i+3]<='9'&&file[i+4]=='-'){
        date_like=1;

        size_t n=0;
        int err=0;
        long tz=0;
        unsigned long stamp=confc_ex_str_to_date(&file[i],&tz,&n,&err);
        if(err!=2&&n>0&&toml_value_end(file[i+n])){
            toml_datetime_t *dt=malloc(sizeof(toml_datetime_t));
            if(dt==NULL){
                confc_log_err("toml_parse_value: malloc %zu bytes failed: %s\n",sizeof(toml_datetime_t),strerror(errno));
                return NULL;
            }
            dt->tp=TOML_DATETIME;
            dt->time=stamp;
            dt->offset=tz;
            i+=n;
            return (toml_type_t *)dt;
        }
    }
    if(file[i]>='0'&&file[i]<='9'&&file[i+1]>='0'&&file[i+1]<='9'&&file[i+2]==':'){
        date_like=1;
    }
    if(date_like){
        return NULL;
    }

    size_t n=0;
    int err=0;

    /* 整数 */
    long iv=confc_ex_str_to_int(&file[i],&n,&err,1);
    if(err!=2&&n>0&&toml_value_end(file[i+n])){
        toml_integer_t *it=malloc(sizeof(toml_integer_t));
        if(it==NULL){
            confc_log_err("toml_parse_value: malloc %zu bytes failed: %s\n",sizeof(toml_integer_t),strerror(errno));
            return NULL;
        }
        it->tp=TOML_INTEGER;
        it->v=iv;
        i+=n;
        return (toml_type_t *)it;
    }

    /* 浮点（含 inf / nan） */
    double fv=confc_ex_str_to_float(&file[i],&n,&err,1);
    if(err!=2&&n>0&&toml_value_end(file[i+n])){
        toml_float_t *ft=malloc(sizeof(toml_float_t));
        if(ft==NULL){
            confc_log_err("toml_parse_value: malloc %zu bytes failed: %s\n",sizeof(toml_float_t),strerror(errno));
            return NULL;
        }
        ft->tp=TOML_FLOAT;
        ft->v=fv;
        i+=n;
        return (toml_type_t *)ft;
    }

    return NULL;

#undef i
}

/* 解析一个数组，元素交给 toml_parse_value（所以数组里可以再套数组） */
static void toml_parse_array(toml_array_t *arr,const char *file,size_t *i){
#define i (*i)

    if(file[i]!='['){
        return ;
    }
    i++;

    for(;;){
        /* 跳过空白和元素之间的逗号 */
        while(file[i]==' '||file[i]=='\t'||file[i]=='\r'||file[i]=='\n'||file[i]==','){
            i++;
        }
        if(file[i]=='\0'||file[i]==']'){
            break;
        }

        toml_type_t *v=toml_parse_value(file,&i);
        if(v==NULL){
            break;
        }

        toml_type_t **items=realloc(arr->items,(arr->size+1)*sizeof(toml_type_t *));
        if(items==NULL){
            confc_log_err("toml_parse_array: realloc %zu bytes failed: %s\n",(size_t)((arr->size+1)*sizeof(toml_type_t *)),strerror(errno));
            toml_free_value(v);
            break;
        }
        arr->items=items;
        arr->items[arr->size++]=v;
    }

    if(file[i]==']'){
        i++;
    }

#undef i
}

static void toml_commit_table(toml_t *ret,toml_table_t **now_table){
    toml_table_t *cur=*now_table;
    if(cur==NULL){
        return ;
    }
    *now_table=NULL;

    char *old_name=(char *)cur->name;

    if(cur->tp!=TOML_ARRAY_OF_TABLES){
        toml_add_table(ret,cur);
        free(old_name);
        return ;
    }

    toml_array_of_tables_t *aot=malloc(sizeof(toml_array_of_tables_t));
    struct toml_table_item_t *items=malloc(sizeof(struct toml_table_item_t));
    if(aot==NULL||items==NULL){
        confc_log_err("toml_commit_table: malloc failed: %s\n",strerror(errno));
        free(aot);
        free(items);
        cur->tp=TOML_TABLE;
        toml_free_value((toml_type_t *)cur);
        free(old_name);
        return ;
    }

    aot->tp=TOML_ARRAY_OF_TABLES;
    aot->name=old_name;
    aot->table=items;
    aot->size=1;
    items[0].pairs=cur->pairs;
    items[0].size=cur->size;

    /* 同名的是普通 table：这一段没地方放，直接释放 */
    toml_type_t *existing=toml_lookup_table(ret,old_name);
    if(existing!=NULL&&*existing!=TOML_ARRAY_OF_TABLES){
        confc_log_err("toml_commit_table: \"%s\" already exists as a table\n",old_name);
        cur->tp=TOML_TABLE;
        toml_free_value((toml_type_t *)cur);
        free(aot->table);
        free(aot);
        free(old_name);
        return ;
    }

    toml_add_table(ret,(toml_table_t *)aot);

    free(cur);
    free(old_name);
}

static void toml_parse_per_pair(toml_t *ret,toml_table_t **now_table,const char *file,size_t *i){
#define i (*i)

    da_str key;
    da_init(&key);

    /* 跳过空白 */
    while(file[i]==' '||file[i]=='\t'||file[i]=='\r'||file[i]=='\n'){
        i++;
    }

    /* 空行 / 注释 */
    if(file[i]=='#'||file[i]=='\0'){
        while(file[i]!='\0'&&file[i]!='\n'){
            i++;
        }
        da_clear(&key);
        return ;
    }

    /* 表头：[name] 或 [[name]]；先把上一段表挂到根上，再开一个新的 */
    if(file[i]=='['){
        int aot=(file[i+1]=='[');
        i+=aot?2:1;

        /* 表名：允许 a.b 这样的多段，也允许带引号的段；这里拼成一层名字（如 "a.b"） */
        for(;;){
            while(file[i]==' '||file[i]=='\t'){
                i++;
            }

            if(file[i]=='\"'||file[i]=='\''){
                size_t used=0;
                toml_type_t *s=toml_parse_string(&file[i],&used);
                if(s!=NULL){
                    for(const char *q=((toml_string_t *)s)->str;*q!='\0';q++){
                        da_add(sizeof(char),&key,q);
                    }
                    free(((toml_string_t *)s)->str);
                    free(s);
                }
                i+=used;
            }else{
                while(file[i]!='\0'&&file[i]!='.'&&file[i]!=']'&&file[i]!='\r'&&file[i]!='\n'&&file[i]!=' '&&file[i]!='\t'){
                    da_add(sizeof(char),&key,&file[i]);
                    i++;
                }
            }

            while(file[i]==' '||file[i]=='\t'){
                i++;
            }
            if(file[i]!='.'){
                break;
            }
            i++;
            da_add(sizeof(char),&key,".");
        }
        da_add(sizeof(char),&key,"");

        if(file[i]==']'){
            i++;
            if(aot&&file[i]==']'){
                i++;
            }
        }
        while(file[i]!='\0'&&file[i]!='\n'&&file[i]!='\r'){
            i++;
        }

        toml_commit_table(ret,now_table);

        toml_table_t *table=malloc(sizeof(toml_table_t));
        if(table==NULL){
            confc_log_err("toml_parse_per_pair: malloc %zu bytes failed: %s\n",sizeof(toml_table_t),strerror(errno));
            da_clear(&key);
            return ;
        }
        /* tp 顺便当标记：TOML_ARRAY_OF_TABLES 表示这一段来自 [[name]]，
           toml_commit_table 会据此把它包成 AOT 的一个元素 */
        *table=(toml_table_t){.tp=aot?TOML_ARRAY_OF_TABLES:TOML_TABLE,.name=key.arr,.pairs=NULL,.size=0};
        *now_table=table;
        return ;
    }

    /* 键：可以带引号，也可以裸写 */
    if(file[i]=='\"'||file[i]=='\''){
        char q=file[i];
        int literal=(q=='\'');
        i++;
        while(file[i]!='\0'&&file[i]!=q){
            if(literal||file[i]!='\\'){
                da_add(sizeof(char),&key,&file[i]);
                i++;
                continue;
            }

            size_t n=0;
            char *r=confc_ex_parse_esc_all(&file[i],&n);
            if(r==NULL){
                i++;
                continue;
            }
            for(size_t k=0;r[k]!='\0';k++){
                da_add(sizeof(char),&key,&r[k]);
            }
            free(r);
            i+=n;
        }
        if(file[i]==q){
            i++;
        }
    }else{
        while(file[i]!='\0'&&file[i]!=' '&&file[i]!='\t'&&file[i]!='\r'&&file[i]!='\n'&&file[i]!='='&&file[i]!='#'){
            da_add(sizeof(char),&key,&file[i]);
            i++;
        }
    }
    da_add(sizeof(char),&key,"");

    while(file[i]==' '||file[i]=='\t'){
        i++;
    }

    if(file[i]!='='){
        /* 不是 key = value，丢掉这一行 */
        while(file[i]!='\0'&&file[i]!='\n'){
            i++;
        }
        da_clear(&key);
        return ;
    }
    i++;

    while(file[i]==' '||file[i]=='\t'){
        i++;
    }

    /* 值：交给 toml_parse_value 统一解析 */
    toml_type_t *v=toml_parse_value(file,&i);

    /* 这一行剩下的东西（注释等）跳过 */
    while(file[i]!='\0'&&file[i]!='\n'){
        i++;
    }

    if(v==NULL){
        da_clear(&key);
        return ;
    }

    /* 根层的键放到一个名字为 "" 的表里 */
    if(*now_table==NULL){
        toml_table_t *table=malloc(sizeof(toml_table_t));
        char *name=malloc(1);
        if(table==NULL||name==NULL){
            confc_log_err("toml_parse_per_pair: malloc failed: %s\n",strerror(errno));
            free(table);
            free(name);
            toml_free_value(v);
            da_clear(&key);
            return ;
        }
        name[0]='\0';
        *table=(toml_table_t){.tp=TOML_TABLE,.name=name,.pairs=NULL,.size=0};
        *now_table=table;
    }

    /* dotted key：a.b.c = v 的表名直接就是 a.b（根下只有一层表，不嵌套） */
    char *dotp=strrchr(key.arr,'.');
    char *part=key.arr;
    toml_table_t *target=*now_table;
    if(dotp!=NULL&&dotp!=key.arr){
        *dotp='\0';
        part=dotp+1;

        toml_type_t *sub=toml_lookup_table(ret,key.arr);
        if(sub==NULL||*sub!=TOML_TABLE){
            toml_table_t *table=malloc(sizeof(toml_table_t));
            if(table==NULL){
                confc_log_err("toml_parse_per_pair: malloc %zu bytes failed: %s\n",sizeof(toml_table_t),strerror(errno));
                toml_free_value(v);
                da_clear(&key);
                return ;
            }
            *table=(toml_table_t){.tp=TOML_TABLE,.name=key.arr,.pairs=NULL,.size=0};

            toml_add_table(ret,table);   /* name 会被拷到根上并把 table->name 指回去 */
            sub=(toml_type_t *)table;
        }
        target=(toml_table_t *)sub;
    }

    toml_pair_t pair={.key=part,.value=v};
    toml_add_pair(target,&pair);

    da_clear(&key);

#undef i
}

toml_t toml_load_from_memory(const char *file){
    toml_t ret={0};
    if(file==NULL){
        return ret;
    }

    toml_table_t *now_table=NULL;
    size_t i=0;
    while(file[i]!='\0'){
        size_t before=i;
        toml_parse_per_pair(&ret,&now_table,file,&i);
        if(i==before){
            i++;   /* 保证一定前进，避免死循环 */
        }
    }

    /* 收尾：把最后一段表挂到根上 */
    toml_commit_table(&ret,&now_table);

    return ret;
}

void toml_unload(toml_t *file){
    if(file==NULL){
        return ;
    }

    for(size_t i=0;i<file->size;i++){
        free(file->pairs[i].key);
        toml_free_value(file->pairs[i].value);
    }
    free(file->pairs);
    file->pairs=NULL;
    file->size=0;
}

void toml_add_table(toml_t *file,toml_table_t *table){
    if(file==NULL||table==NULL){
        confc_log_err("toml_add_table: nullptr argument\n");
        return;
    }

    /* 表名来自表对象本身，按 table->tp 判断是 table 还是 array of tables */
    const char *src_key=NULL;
    if(table->tp==TOML_TABLE){
        src_key=table->name;
    }else if(table->tp==TOML_ARRAY_OF_TABLES){
        src_key=((toml_array_of_tables_t *)table)->name;
    }else{
        confc_log_err("toml_add_table: not a table\n");
        return;
    }
    if(src_key==NULL){
        src_key="";
    }

    char *key=malloc(strlen(src_key)+1);
    if(!key){
        confc_log_err("toml_add_table: malloc %zu bytes failed: %s\n",(size_t)(strlen(src_key)+1),strerror(errno));
        return;
    }
    strcpy(key,src_key);

    if(file->size==0){
        file->pairs=malloc(sizeof(toml_pair_t));
        if(!file->pairs){
            confc_log_err("toml_add_table: malloc %zu bytes failed: %s\n",sizeof(toml_pair_t),strerror(errno));
            free(key);
            return;
        }
        file->pairs[0].key=key;
        file->pairs[0].value=(toml_type_t *)table;
        if(table->tp==TOML_TABLE){
            table->name=key;
        }else{
            ((toml_array_of_tables_t *)table)->name=key;
        }
        file->size++;
        return;
    }

    size_t l=0,r=file->size-1;
    size_t mid=l+(r-l)/2;
    while(l<=r&&r!=(size_t)-1){
        mid=l+(r-l)/2;
        int res=strcmp(file->pairs[mid].key,key);
        if(res==0){
            if(file->pairs[mid].value==(toml_type_t *)table){
                /* 同一个对象重复加入：已经在根上，直接忽略 */
                free(key);
                return;
            }
            if(table->tp==TOML_ARRAY_OF_TABLES){
                /* 同名：已有 AOT 就把新对象里的元素并进数组；已有 table 就报错 */
                if(file->pairs[mid].value!=NULL&&*file->pairs[mid].value==TOML_ARRAY_OF_TABLES){
                    toml_array_of_tables_t *dst=(toml_array_of_tables_t *)file->pairs[mid].value;
                    toml_array_of_tables_t *src=(toml_array_of_tables_t *)table;
                    if(src->size>0){
                        struct toml_table_item_t *items=realloc(dst->table,(dst->size+src->size)*sizeof(struct toml_table_item_t));
                        if(items==NULL){
                            confc_log_err("toml_add_table: realloc %zu bytes failed: %s\n",(size_t)((dst->size+src->size)*sizeof(struct toml_table_item_t)),strerror(errno));
                            free(key);
                            toml_free_value((toml_type_t *)table);
                            return;
                        }
                        dst->table=items;
                        for(size_t k=0;k<src->size;k++){
                            dst->table[dst->size+k]=src->table[k];
                        }
                        dst->size+=src->size;
                    }
                    /* 元素已经交给 dst，回收 src 的壳（pairs 不能动，已经被 dst 接管） */
                    free(src->table);
                    free(src);
                }else{
                    confc_log_err("toml_add_table: \"%s\" already exists as a table, cannot add array of tables\n",key);
                    toml_free_value((toml_type_t *)table);
                }
                free(key);
                return;
            }
            confc_log_err("toml_add_table: duplicate key \"%s\", overwritten\n",key);
            free(key);
            toml_free_value(file->pairs[mid].value);   /* 覆盖前把旧值释放掉 */
            file->pairs[mid].value=(toml_type_t *)table;
            if(table->tp==TOML_TABLE){
                table->name=file->pairs[mid].key;
            }else{
                ((toml_array_of_tables_t *)table)->name=file->pairs[mid].key;
            }
            return;
        }else if(res<0){
            l=mid+1;
        }else{
            r=mid-1;
        }
    }

    toml_pair_t *pairs=realloc(file->pairs,(file->size+1)*sizeof(toml_pair_t));
    if(!pairs){
        confc_log_err("toml_add_table: realloc %zu bytes failed: %s\n",(size_t)((file->size+1)*sizeof(toml_pair_t)),strerror(errno));
        free(key);
        return;
    }
    file->pairs=pairs;
    memmove(&file->pairs[l+1],&file->pairs[l],(file->size-l)*sizeof(toml_pair_t));
    file->pairs[l].key=key;
    file->pairs[l].value=(toml_type_t *)table;
    if(table->tp==TOML_TABLE){
        table->name=key;
    }else{
        ((toml_array_of_tables_t *)table)->name=key;
    }
    file->size++;
}

void toml_add_pair(toml_table_t *table,toml_pair_t *pair){
    if(table==NULL||pair==NULL){
        confc_log_err("toml_add_pair: nullptr argument\n");
        return;
    }

    const char *src_key=pair->key?pair->key:"";
    char *key=malloc(strlen(src_key)+1);
    if(!key){
        confc_log_err("toml_add_pair: malloc %zu bytes failed: %s\n",(size_t)(strlen(src_key)+1),strerror(errno));
        return;
    }
    strcpy(key,src_key);

    if(table->size==0){
        table->pairs=malloc(sizeof(toml_pair_t));
        if(!table->pairs){
            confc_log_err("toml_add_pair: malloc %zu bytes failed: %s\n",sizeof(toml_pair_t),strerror(errno));
            free(key);
            return;
        }
        table->pairs[0].key=key;
        table->pairs[0].value=pair->value;
        table->size++;
        return;
    }

    size_t l=0,r=table->size-1;
    size_t mid=l+(r-l)/2;
    while(l<=r&&r!=(size_t)-1){
        mid=l+(r-l)/2;
        int res=strcmp(table->pairs[mid].key,key);
        if(res==0){
            confc_log_err("toml_add_pair: duplicate key \"%s\", overwritten\n",key);
            free(key);
            if(table->pairs[mid].value!=pair->value){
                toml_free_value(table->pairs[mid].value);
            }
            table->pairs[mid].value=pair->value;
            return;
        }else if(res<0){
            l=mid+1;
        }else{
            r=mid-1;
        }
    }

    toml_pair_t *pairs=realloc(table->pairs,(table->size+1)*sizeof(toml_pair_t));
    if(!pairs){
        confc_log_err("toml_add_pair: realloc %zu bytes failed: %s\n",(size_t)((table->size+1)*sizeof(toml_pair_t)),strerror(errno));
        free(key);
        return;
    }
    table->pairs=pairs;
    memmove(&table->pairs[l+1],&table->pairs[l],(table->size-l)*sizeof(toml_pair_t));
    table->pairs[l].key=key;
    table->pairs[l].value=pair->value;
    table->size++;
}


toml_type_t *toml_lookup(toml_t *file,const char *table,size_t i,const char *key){
    if(file==NULL||key==NULL){
        return NULL;
    }

    if(table==NULL||table[0]=='\0'){
        /* 根层的键放在名字为 "" 的表里（和 ini 的根 section 一致） */
        toml_type_t *root=toml_lookup_table(file,"");
        if(root!=NULL&&*root==TOML_TABLE){
            return toml_lookup_from_table((toml_table_t *)root,key);
        }
        return NULL;
    }

    toml_type_t *found=toml_lookup_table(file,table);
    if(found==NULL){
        return NULL;
    }
    /* 每个值结构体都以 toml_type_t tp 开头，解引用即得类型 */
    toml_type_t tp=*found;
    if(tp==TOML_TABLE){
        return toml_lookup_from_table((toml_table_t *)found,key);
    }
    if(tp==TOML_ARRAY_OF_TABLES){
        return toml_lookup_from_array_of_tables((toml_array_of_tables_t *)found,i,key);
    }
    return NULL;
}

toml_type_t *toml_lookup_table(toml_t *file,const char *name){
    if(file==NULL||name==NULL||file->size==0){
        return NULL;
    }

    size_t l=0,r=file->size-1;
    while(l<=r&&r!=(size_t)-1){
        size_t mid=l+(r-l)/2;
        int res=strcmp(file->pairs[mid].key,name);
        if(res==0){
            return file->pairs[mid].value;
        }else if(res<0){
            l=mid+1;
        }else{
            r=mid-1;
        }
    }
    return NULL;
}

toml_type_t *toml_lookup_from_table(toml_table_t *table,const char *key){
    if(table==NULL||key==NULL||table->size==0){
        return NULL;
    }

    size_t l=0,r=table->size-1;
    while(l<=r&&r!=(size_t)-1){
        size_t mid=l+(r-l)/2;
        int res=strcmp(table->pairs[mid].key,key);
        if(res==0){
            return table->pairs[mid].value;
        }else if(res<0){
            l=mid+1;
        }else{
            r=mid-1;
        }
    }
    return NULL;
}

toml_type_t *toml_lookup_from_array_of_tables(toml_array_of_tables_t *table,size_t i,const char *key){
    if(table==NULL||key==NULL||i>=table->size||table->table==NULL){
        return NULL;
    }

    toml_table_t item={.tp=TOML_TABLE,.name=table->name,.pairs=table->table[i].pairs,.size=table->table[i].size};
    return toml_lookup_from_table(&item,key);
}



#ifndef DARRAY_H

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
            confc_log_err("da_add: realloc %zu bytes failed: %s\n",real*tp_size,strerror(errno));
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

int da_clear(void *array){
    da_generic_t header;
    da_load_header(array,&header);
    free(header.arr);
    memset(array,0,sizeof(da_generic_t));
    return 0;
}

#endif

#undef confc_log_err
#undef toml_value_end

#endif

#endif