/* 输入框布局测试：
 * - 输入框本身只画文字，背景/边框/光标由本程序绘制
 * - 鼠标移入切换 I-beam，左键聚焦，点击外部失焦
 * - 横向滚轮在文字溢出时滚动文字
 * - Enter 提交，Esc 先失焦；没有焦点时 Esc 退出
 *
 * 运行：make test-input
 */
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include "sdlgui.h"

#define FIELD_X 120.0f
#define FIELD_Y 130.0f
#define FIELD_W 400.0f
#define FIELD_H 52.0f
#define FIELD_MAX_BYTES 96

static void on_submit(const char *text, void *data)
{
    (void)data;
    SDL_Log("input submitted: \"%s\"", text);
}

static void set_color(unsigned color)
{
    SDL_SetRenderDrawColor(get_renderer(),
        (Uint8)((color >> 24) & 0xFF),
        (Uint8)((color >> 16) & 0xFF),
        (Uint8)((color >> 8) & 0xFF),
        (Uint8)(color & 0xFF));
}

static void draw_caret(const layout_input_t *input, TTF_Font *font)
{
    if (!layout_input_is_focused(input)) {
        return;
    }

    const char *text = layout_input_text(input);
    size_t cursor = layout_input_cursor(input);
    int width = 0;
    int height = 0;
    /* TTF_GetStringSize(...,0,...) 会测量整段文字；cursor=0 时前缀宽度必须为 0。 */
    if (cursor > 0 && !TTF_GetStringSize(font, text, cursor, &width, &height)) {
        return;
    }

    float scale = sdlgui_get_scale();
    if (scale <= 0.0f) {
        scale = 1.0f;
    }

    float line_height = (float)TTF_GetFontHeight(font) / scale;
    float x = input->l.x - input->text_scroll + (float)width / scale;
    float y = input->l.y + (input->l.height - line_height) / 2.0f;

    if (x < input->l.x || x > input->l.x + input->l.width) {
        return;
    }

    SDL_FRect caret = {x, y, 2.0f, line_height};
    set_color(get_text_color(TEXT_CONTEXT));
    SDL_RenderFillRect(get_renderer(), &caret);
}

static void render(const layout_rect_t *field, const layout_input_t *input,
                   TTF_Font *font)
{
    SDL_Renderer *renderer = get_renderer();

    set_color(get_color(COLOR_MAIN));
    SDL_RenderClear(renderer);

    update_root();
    draw_root();

    if (layout_input_is_focused(input)) {
        SDL_FRect border = {
            field->l.x - 2.0f,
            field->l.y - 2.0f,
            field->l.width + 4.0f,
            field->l.height + 4.0f
        };
        set_color(get_color(COLOR_BUTTON_HIGHLIGHT));
        SDL_RenderRect(renderer, &border);
    }

    draw_caret(input, font);
    SDL_RenderPresent(renderer);
}

/* 主题的 monospace 在 macOS 上通常是 Menlo，没有中文字形。
 * 测试程序显式找一个支持中文的字体；找不到再回退主题字体。 */
static TTF_Font *load_test_font(void)
{
    static const char *names[] = {
        "PingFang SC",
        "Noto Sans CJK SC",
        "Microsoft YaHei",
        "WenQuanYi Zen Hei",
        "monospace"
    };

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        TTF_Font *font = obtain_font(names[i], get_text_size(TEXT_CONTEXT));
        if (font != NULL) {
            if (i > 0) {
                SDL_Log("input-test: using fallback CJK font \"%s\"", names[i]);
            }
            return font;
        }
    }
    return NULL;
}

int main(void)
{
    sdlgui_init("input-test", 640, 360, NULL, NULL);
    if (get_window() == NULL) {
        return 1;
    }

    update_root();

    align_t zero = make_align(0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    TTF_Font *font = load_test_font();
    if (font == NULL) {
        sdlgui_quit();
        return 1;
    }

    layout_label_t *hint = create_layout_label(
        get_root_layout(),
        500.0f, 86.0f, zero, zero,
        font,
        "Click the input field and type.\n"
        "Horizontal wheel scrolls the text when it overflows.\n"
        "Enter submits. Esc blurs; press Esc again to quit.",
        get_text_color(TEXT_CONTEXT)
    );

    layout_rect_t *field = create_layout_rect(
        get_root_layout(), FIELD_W, FIELD_H, zero);
    layout_input_t *input = create_layout_input(
        get_root_layout(),
        FIELD_W, FIELD_H, zero,
        "中文输入测试：The quick brown fox jumps over the lazy dog",
        font,
        get_text_color(TEXT_CONTEXT),
        FIELD_MAX_BYTES,
        on_submit,
        NULL
    );

    if (hint == NULL || field == NULL || input == NULL) {
        sdlgui_quit();
        return 1;
    }

    hint->l.x = 70.0f;
    hint->l.y = 24.0f;

    field->l.x = FIELD_X;
    field->l.y = FIELD_Y;
    field->color = get_color(COLOR_SECONDARY);

    input->l.x = FIELD_X;
    input->l.y = FIELD_Y;
    input->cursor = 0;          /* 让初始光标从行首开始，便于观察 */
    input->text_scroll = 0.0f;

    update_root();

    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (!sdlgui_handle_event(&event)) {
                running = 0;
            }
        }

        field->color = layout_input_is_focused(input)
            ? get_color(COLOR_BUTTON)
            : get_color(COLOR_SECONDARY);

        render(field, input, font);
    }

    sdlgui_quit();
    return 0;
}
