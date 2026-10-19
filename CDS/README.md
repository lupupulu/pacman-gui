# CDS

一组**单头文件**形式的 C 工具库。每个头文件都可以直接拷进项目里用：包含它，并在**一个** `.c` 文件里定义对应的 `*_IMPLEMENTATION` 宏即可，不需要编译/链接额外的目标文件。

| 头文件 | 内容 |
|---|---|
| `confc.h` | INI / TOML 配置的解析、查找与写出；路径查找、转义解析、字符串→数字/日期转换 |
| `darray.h` | 动态数组（`da_*`），`confc.h` 会优先使用它 |
| `input.h` | 终端行输入 / 按键处理 |
| `bitmap.h` | 位图 |

单头文件的写法参考 [stb](https://github.com/nothings/stb)：`#ifndef XXX_H` 守卫 + `#ifdef XXX_IMPLEMENTATION` 包住实现，因此同一个头文件被多个源文件包含也不会重复定义。

## 快速开始

```c
/* 一个（而且只应该是一个）.c 文件里 */
#define DARRAY_IMPLEMENTATION
#include "darray.h"

#define CONFC_IMPLEMENTATION
#include "confc.h"

/* 其它文件里只要 */
#include "confc.h"
```

没有 `darray.h` 也能用：`confc.h` 自带一份最小的动态数组实现，直接 `#define CONFC_IMPLEMENTATION` 即可。

## confc.h

### 特性

- **C99**：`gcc -std=c99 -pedantic-errors -Wall -Wextra` 零错误；不依赖 GNU 扩展。
- **无外部依赖**：只用标准库和少量 POSIX 接口（见"兼容性"）。
- **INI**：读文件 / 从内存解析 / 写出（`FILE*`、`fd`、内存三选一）/ 增删改查；section 与 pair 都保持**有序**，查找走二分。
- **TOML**：表、数组表（`[[x]]`）、数组（可嵌套）、基本/字面量/多行字符串、整数、浮点、布尔、日期时间；查找同样走二分。
- **工具**：`confc_find_path`（支持 `~`、`~user`、`$VAR`、`${VAR}`）、转义解析（含 `\uXXXX` / `\UXXXXXXXX`）、字符串→整数/浮点/日期（宽松 / 严格两种模式）。

### INI

```c
ini_t ini=ini_load("config.ini");
const char *host=ini_lookup(&ini,"server","host");
printf("host = %s\n",host?host:"(未设置)");

/* 改一个键（key/value 会被拷贝） */
ini_pair_t pair={.key="port",.value="9090"};
ini_add_pair(ini_lookup_section(&ini,"server"),&pair);

/* 写回：FILE* / fd / 内存 */
ini_write(stdout,&ini);
ini_unload(&ini);
```

结构：

```c
typedef struct { char *name; size_t pair_size; ini_pair_t *pairs; } ini_section_t;
typedef struct { size_t section_size; ini_section_t *sections; } ini_t;
```

| 函数 | 说明 |
|---|---|
| `ini_load` / `ini_load_from_memory` | 读文件 / 从内存解析 |
| `ini_unload` | 释放整棵树 |
| `ini_write` / `ini_write_fd` / `ini_write_to_memory` | 写到 `FILE*` / `fd` / 新分配的内存（调用者 `free`） |
| `ini_add_section` / `ini_add_pair` | 加 section / 键值（名字与值都会被拷贝） |
| `ini_del_section` / `ini_del_pair` / `ini_del_pair_from_section` | 删除 |
| `ini_lookup` / `ini_lookup_section` / `ini_lookup_from_section` | 查找 |

> 注意：`ini_add_section` 返回的是 `file->sections` 里的指针，之后再添加 section 可能因为 `realloc` 失效，请重新查找。

### TOML

```c
toml_t f=toml_load("config.toml");

/* 根层的键放在名字为 "" 的表里，table 传 NULL 即可 */
toml_type_t *v=toml_lookup(&f,NULL,0,"title");
if(v!=NULL&&*v==TOML_STRING){
    printf("title = %s\n",((toml_string_t *)v)->str);
}

/* 表：toml_lookup(文件, "表名", 数组表的第几个元素, "键") */
v=toml_lookup(&f,"server",0,"port");
if(v!=NULL&&*v==TOML_INTEGER){
    printf("port = %ld\n",((toml_integer_t *)v)->v);
}

/* 数组 */
v=toml_lookup(&f,"server",0,"tags");
if(v!=NULL&&*v==TOML_ARRAY){
    toml_array_t *arr=(toml_array_t *)v;
    for(size_t i=0;i<arr->size;i++){
        if(*arr->items[i]==TOML_STRING){
            printf("tag: %s\n",((toml_string_t *)arr->items[i])->str);
        }
    }
}

/* 数组表：[[jobs]] 的第 0 个元素 */
v=toml_lookup(&f,"jobs",0,"cmd");

toml_unload(&f);
```

数据都是普通结构体，第一个字段永远是 `toml_type_t tp`：

```c
typedef enum {
    TOML_STRING, TOML_INTEGER, TOML_FLOAT, TOML_BOOLEAN,
    TOML_DATETIME, TOML_ARRAY, TOML_TABLE, TOML_ARRAY_OF_TABLES
} toml_type_t;

typedef struct { toml_type_t tp; char *str; } toml_string_t;
typedef struct { toml_type_t tp; long v; } toml_integer_t;
typedef struct { toml_type_t tp; double v; } toml_float_t;
typedef struct { toml_type_t tp; int v; } toml_boolean_t;
typedef struct { toml_type_t tp; unsigned long time; long offset; } toml_datetime_t;
typedef struct { toml_type_t tp; toml_type_t **items; size_t size; } toml_array_t;
typedef struct { toml_type_t tp; const char *name; toml_pair_t *pairs; size_t size; } toml_table_t;
```

`toml_datetime_t.time` 是 Unix 时间戳（秒），`offset` 是相对 UTC 的偏移（秒，东为正）。

| 函数 | 说明 |
|---|---|
| `toml_load` / `toml_load_from_memory` | 读文件 / 从内存解析 |
| `toml_unload` | 递归释放 |
| `toml_add_table` / `toml_add_pair` | 手动建树（key 会被拷贝） |
| `toml_lookup` | `(文件, 表名, 数组表下标, 键)`，表名传 `NULL`/`""` 表示根层的键 |
| `toml_lookup_table` | 按名字取根上的表（或数组表） |
| `toml_lookup_from_table` / `toml_lookup_from_array_of_tables` | 从已有的表 / 数组表元素里取键 |

#### 支持范围

- 表头 `[a]`、`[[a]]`、带引号的表名 `["a b"]`、点号表头 `[a.b]`（按**一层**处理，名字就是 `a.b`）；
- 键：裸键、带引号的键、dotted key（`a.b = 1`，表名取最后一个点之前的部分，如 `a.b`）；
- 值：字符串（`"..."`、`'...'`、`"""..."""`、`'''...'''`）、整数（含 `0x`/`0o`/`0b`、`_` 分隔）、浮点（含 `inf`/`nan`）、布尔、日期时间（`1979-05-27`、`1979-05-27T07:32:00Z`、带 `±HH:MM`、可带小数秒）、数组（元素可以是任意类型，可嵌套）。

#### 已知限制

- **内联表** `{ key = value }` 还不支持（`pyproject.toml`、`Cargo.toml` 里常见）。
- `[a.b]` 风格的子表不会嵌进上一层的 `[[a]]` 元素里，而是作为根上的扁平表 `a.b`（udev 键表的 `[protocols.scancodes]` 就是这种）。
- 本地时间 `07:32:00` 不支持。
- 表里的 dotted key 会建到根上（`[sec]` 里写 `x.y = 1` 得到根上的表 `x.y`，而不是 `sec.x.y`）。
- 同名表/键重复出现时按"后写覆盖"处理，会打一条 `confc_log_err`。

### 工具函数

```c
char *ls=confc_find_path(getenv("PATH"),"ls");   /* 支持 ~ ~user $VAR ${VAR} */
printf("%s\n",ls);
free(ls);

long v=confc_str_to_int("0x1f");                 /* 宽松模式，C 规则 */
long t=confc_ex_str_to_int("1_000",&n,&err,1);   /* 严格模式，TOML 规则 */

char *s=confc_parse_esc_unicode("u4e2d");        /* -> "中"，调用者 free（参数从反斜杠后面开始） */
```

`*_ex` 系列统一用 `size_t *n` 报告"用掉了多少字符"、`int *error` 报告状态：

- `0`：顺利；
- `1`：解析成功，但后面还有多余字符；
- `2`：没解析出东西（不完整 / 非法）。

## 兼容性

- 语言：**C99**（`gcc -std=c99 -pedantic-errors -Wall -Wextra` 通过）。没有用 GNU/C23 扩展（`0b...` 二进制字面量、`={}` 空初始化等都已避免）。
- 平台：POSIX。实现里用到 `<unistd.h>`、`<fcntl.h>`、`<pwd.h>`（文件访问、`~user` 展开）。
- 不做 C++ 适配：C++ 里直接包含会因为 `malloc` 返回值需要转换而报错，如果要在 C++ 里用请自己包一层。
- 可选依赖：`darray.h`（没有就用 `confc.h` 内置的最小实现）。

## 构建与测试

库本身没有构建步骤，把需要的头文件拷进项目即可。编译检查：

```sh
# 只检查这一个头文件
printf '#define CONFC_IMPLEMENTATION\n#include "confc.h"\nint main(void){return 0;}\n' > t.c
gcc -std=c99 -pedantic-errors -Wall -Wextra -I. -o t t.c

# 可选：用 ASan/UBSan 跑一下自己的用例
gcc -std=c99 -fsanitize=address,undefined -g -I. -o t t.c && ./t
```

开发时用 Python 的 `tomllib` 做过对照：把解析结果拍平成 `路径<TAB>值` 和 `tomllib` 逐行 diff，本机上两千多个真实 `.toml`（各种配置、udev 键表、Cargo/pyproject 等）里，除上面列出的"内联表""数组表嵌套表头"两类外都能对上。

## 许可

MIT，见 [LICENSE](LICENSE)。
