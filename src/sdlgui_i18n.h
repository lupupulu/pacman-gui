#ifndef SDLGUI_I18N_H
#define SDLGUI_I18N_H

#ifndef SDLGUI_TEXT_DOMAIN
#define SDLGUI_TEXT_DOMAIN "pacman-gui"
#endif

/* 相对当前目录的本地化目录；打包时可用 -DSDLGUI_LOCALEDIR=... 覆盖。
 * macOS 应用包会在运行时优先查找 Contents/Resources/locale。 */
#ifndef SDLGUI_LOCALEDIR
#define SDLGUI_LOCALEDIR "locale"
#endif

/* 有 libintl 就使用真正的 gettext；macOS 默认没有 libintl，
 * 未安装 gettext 时退化为直接返回英文 msgid（界面仍然可用）。 */
#ifndef SDLGUI_HAVE_GETTEXT
#if !defined(SDLGUI_NO_GETTEXT) && defined(__has_include)
#if __has_include(<libintl.h>)
#define SDLGUI_HAVE_GETTEXT 1
#endif
#endif
#endif

#ifdef SDLGUI_HAVE_GETTEXT
#include <libintl.h>
#else
/* 没有 gettext 时直接保留字面量：这样 _("...") 仍是字符串字面量，
 * 编译器不会因为把非字面量当格式串而报 -Wformat-security。 */
#define gettext(msgid) (msgid)
#define bindtextdomain(domain,directory) ((void)(domain),(void)(directory))
#define bind_textdomain_codeset(domain,codeset) ((void)(domain),(void)(codeset))
#define textdomain(domain) ((void)(domain))
#endif

#define _(String) gettext(String)

/* 按环境 locale 启用 gettext；由 theme_set() 成功加载主题后调用 */
void sdlgui_i18n_init(void);

#endif
