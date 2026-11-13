#ifndef INIT_H
#define INIT_H

/* 创建主界面布局、selector 和 tabs；成功返回 1。 */
int init_ui(void);
/* 释放 init_ui() 加载的包管理器数据。 */
void init_ui_quit(void);

#endif
