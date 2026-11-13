# AGENTS.md

本文件根据 `src/` 下现有代码总结项目约定，供后续修改和新增文件使用。
优先保持周边代码风格；不要为了格式化而改动无关代码。

## C 代码风格

- 使用 C99，4 个空格缩进，不使用 Tab。
- 函数、`if`、`for`、`while`、`switch` 的左花括号放在同一行：

  ```c
  static void package_update(void *p){
      if(card==NULL){
          return;
      }else{
          ...
      }
  }
  ```

- `else` 紧跟在右花括号后：`}else{`、`}else if(...){`。
- 控制关键字和括号之间不加空格：`if(...)`、`for(...)`、`while(...)`、`switch(...)`。
- 函数调用、参数列表、数组下标中逗号后不加空格：`make_align(0,0,1,1,...)`、`create_layout(a,b,c)`。
- 二元运算符、赋值、比较、三元运算符两侧不加空格：

  ```c
  float width=parent->width*0.5f;
  if(item==NULL||width<=0.0f){
      return NULL;
  }
  ```

- 指针星号靠近类型：`layout_t *l`、`char *name`、`const char *text`。
- 强制类型转换写成 `(layout_t *)card`。
- 结构体初始化使用指定初始化器，`=` 两侧不留空格：

  ```c
  static package_t packages[]={
      {
          .name="pacman",
          .version="6.1.0",
          .installed=1
      }
  };
  ```

- 多行函数调用保持 4 空格续行，末尾 `);` 单独一行：

  ```c
  layout_list_t *list=create_layout_list(
      parent,
      0.0f,
      0.0f,
      make_align(0,0,1,1,0.0f,0.0f,1.0f,1.0f),
      0,
      (layout_t *)cards[0],
      NULL
  );
  ```

- 长条件可以在 `||` / `&&` 后换行，续行统一缩进 4 个空格。
- 空行用于分隔逻辑段落；同一函数内不要把每个语句都用空行隔开。

## 命名与接口

- 类型使用 `_t` 后缀：`layout_t`、`widget_selector_t`、`layout_input_t`。
- 布局构造函数使用 `create_layout_*`：`create_layout_list`、`create_layout_label`、
  `create_layout_input`、`create_layout_package`。
- 控件构造函数沿用现有前缀：
  - `widget_create_*`：主题版本
  - `widget_create_raw_*`：显式传入尺寸/字体/颜色的 raw 版本
  - `set_*`：控件属性设置
- 内部辅助函数使用 `static`，命名为 `xxx_yyy`；文件内部状态也用 `_` 分隔。
- 公开函数和结构体字段要能表达用途；已有公开 API 不要随意改名。

## 注释与日志

- 注释使用中文，复杂逻辑用 `/* ... */` 说明原因，而不是复述代码。
- 新增代码优先使用块注释；已有 `//` 注释不强行改写。
- 用户可见字符串通过 `_()` 包裹，例如：

  ```c
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to create tabs"));
  ```

- 创建失败时返回 `NULL`/`0`，必要时清理已经创建的对象并记录日志。

## 布局与内存约定

- 布局结构体的第一个成员是 `layout_t`（或包含 `layout_t` 的结构），以便统一挂接。
- `layout_attach()` 后布局由父布局管理；删除父布局会递归删除子布局。
- 明确区分“借用”和“拥有”的字符串/纹理：
  - 构造时接管所有权的字段在 `clear` 中释放。
  - 借用字符串用 `const char *` 或注释说明。
- 不要覆盖外部传入控件的 callback；需要接管时先保存再转发。

## 构建与测试

常用命令：

```sh
make
make app
make compdb
make test-input
make OPT=-O2 LTO=1
make OPT=-Os LTO=1
make OPT=-O3 LTO=1
```

- 主程序入口和事件循环在 `src/main.c`；界面创建在 `src/init.c`。
- 输入框交互测试在 `tests/input_test.c`。
- 修改后至少验证默认构建，涉及构建选项时再验证 LTO 组合。
- 提交前运行 `git diff --check`。
