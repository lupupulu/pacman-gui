#ifndef INIT_H
#define INIT_H

/* 创建主界面布局、selector 和 tabs；成功返回 1。
 * 调用前需要先执行 pacman_init_all()，包列表在那时已经准备好。 */
int init_ui(void);
/* 释放 pacman 后端和已安装包数据；应在界面销毁后调用。 */
void init_ui_quit(void);

#endif
