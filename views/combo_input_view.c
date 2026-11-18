#include "combo_input_view.h"
#include <furi.h>
#include <gui/elements.h>
#include <string.h>

#define MAX_VISIBLE_ARROWS 9

typedef struct {
    char header[32];
    uint8_t arrows[MAX_ARROW_COMBO];
    size_t count;
    ComboInputCallback callback;
    ComboCancelCallback cancel_cb;
    void* context;
} ComboInputModel;

struct ComboInputView {
    View* view;
};

static void combo_input_draw_arrow_glyph(Canvas* canvas, int x, int y, uint8_t arrow) {
    // Draw 10x12 rounded box
    canvas_draw_rframe(canvas, x, y, 11, 14, 2);

    switch(arrow) {
    case ArrowUp: // ^
        canvas_draw_line(canvas, x + 5, y + 3, x + 2, y + 7);
        canvas_draw_line(canvas, x + 5, y + 3, x + 8, y + 7);
        canvas_draw_line(canvas, x + 5, y + 3, x + 5, y + 10);
        break;
    case ArrowDown: // v
        canvas_draw_line(canvas, x + 5, y + 10, x + 2, y + 6);
        canvas_draw_line(canvas, x + 5, y + 10, x + 8, y + 6);
        canvas_draw_line(canvas, x + 5, y + 3, x + 5, y + 10);
        break;
    case ArrowLeft: // <
        canvas_draw_line(canvas, x + 2, y + 7, x + 6, y + 3);
        canvas_draw_line(canvas, x + 2, y + 7, x + 6, y + 11);
        canvas_draw_line(canvas, x + 2, y + 7, x + 9, y + 7);
        break;
    case ArrowRight: // >
        canvas_draw_line(canvas, x + 8, y + 7, x + 4, y + 3);
        canvas_draw_line(canvas, x + 8, y + 7, x + 4, y + 11);
        canvas_draw_line(canvas, x + 1, y + 7, x + 8, y + 7);
        break;
    }
}

static void combo_input_view_draw_callback(Canvas* canvas, void* model) {
    ComboInputModel* m = model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, m->header);

    canvas_set_font(canvas, FontSecondary);
    char count_str[32];
    snprintf(count_str, sizeof(count_str), "Count: %zu / %d", m->count, MAX_ARROW_COMBO);
    canvas_draw_str_aligned(canvas, 64, 17, AlignCenter, AlignTop, count_str);

    // Draw visible arrows
    size_t start_idx = 0;
    if(m->count > MAX_VISIBLE_ARROWS) {
        start_idx = m->count - MAX_VISIBLE_ARROWS;
    }

    size_t visible_count = m->count - start_idx;
    int total_width = visible_count * 13;
    int start_x = 64 - (total_width / 2);
    if(start_x < 2) start_x = 2;

    for(size_t i = 0; i < visible_count; i++) {
        combo_input_draw_arrow_glyph(
            canvas,
            start_x + (int)i * 13,
            28,
            m->arrows[start_idx + i]);
    }

    if(m->count == 0) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignTop, "[Use D-Pad arrows]");
    }

    // Bottom hints
    elements_button_left(canvas, "Del");
    elements_button_center(canvas, "OK");
}

static bool combo_input_view_input_callback(InputEvent* event, void* context) {
    ComboInputView* instance = context;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        uint8_t arrow = 0;
        bool is_arrow = false;

        switch(event->key) {
        case InputKeyUp:
            arrow = ArrowUp;
            is_arrow = true;
            break;
        case InputKeyDown:
            arrow = ArrowDown;
            is_arrow = true;
            break;
        case InputKeyLeft:
            arrow = ArrowLeft;
            is_arrow = true;
            break;
        case InputKeyRight:
            arrow = ArrowRight;
            is_arrow = true;
            break;
        case InputKeyOk: {
            ComboInputCallback cb = NULL;
            void* cb_ctx = NULL;
            uint8_t arrows_copy[MAX_ARROW_COMBO];
            size_t count_copy = 0;

            with_view_model(
                instance->view,
                ComboInputModel * m,
                {
                    if(m->count > 0 && m->callback) {
                        cb = m->callback;
                        cb_ctx = m->context;
                        count_copy = m->count;
                        memcpy(arrows_copy, m->arrows, m->count);
                    }
                },
                false);

            if(cb && count_copy > 0) {
                cb(arrows_copy, count_copy, cb_ctx);
            }
            consumed = true;
        } break;
        case InputKeyBack: {
            bool should_cancel = false;
            ComboCancelCallback cancel_cb = NULL;
            void* cb_ctx = NULL;

            with_view_model(
                instance->view,
                ComboInputModel * m,
                {
                    if(m->count > 0) {
                        m->count--;
                    } else {
                        should_cancel = true;
                        cancel_cb = m->cancel_cb;
                        cb_ctx = m->context;
                    }
                },
                true);

            if(should_cancel && cancel_cb) {
                cancel_cb(cb_ctx);
            }
            consumed = true;
        } break;
        default:
            break;
        }

        if(is_arrow) {
            with_view_model(
                instance->view,
                ComboInputModel * m,
                {
                    if(m->count < MAX_ARROW_COMBO) {
                        m->arrows[m->count++] = arrow;
                    }
                },
                true);
            consumed = true;
        }
    }

    return consumed;
}

ComboInputView* combo_input_view_alloc(void) {
    ComboInputView* instance = malloc(sizeof(ComboInputView));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(ComboInputModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, combo_input_view_draw_callback);
    view_set_input_callback(instance->view, combo_input_view_input_callback);

    with_view_model(
        instance->view,
        ComboInputModel * m,
        {
            strncpy(m->header, "Enter Combo", sizeof(m->header));
            m->count = 0;
            m->callback = NULL;
            m->cancel_cb = NULL;
            m->context = NULL;
        },
        false);

    return instance;
}

void combo_input_view_free(ComboInputView* instance) {
    if(!instance) return;
    view_free(instance->view);
    free(instance);
}

View* combo_input_view_get_view(ComboInputView* instance) {
    return instance->view;
}

void combo_input_view_set_header(ComboInputView* instance, const char* header) {
    with_view_model(
        instance->view,
        ComboInputModel * m,
        {
            strncpy(m->header, header, sizeof(m->header) - 1);
            m->header[sizeof(m->header) - 1] = '\0';
        },
        true);
}

void combo_input_view_set_callback(ComboInputView* instance, ComboInputCallback cb, void* context) {
    with_view_model(
        instance->view,
        ComboInputModel * m,
        {
            m->callback = cb;
            m->context = context;
        },
        false);
}

void combo_input_view_set_cancel_callback(ComboInputView* instance, ComboCancelCallback cb, void* context) {
    with_view_model(
        instance->view,
        ComboInputModel * m,
        {
            m->cancel_cb = cb;
            m->context = context;
        },
        false);
}

void combo_input_view_reset(ComboInputView* instance) {
    with_view_model(
        instance->view,
        ComboInputModel * m,
        {
            m->count = 0;
        },
        true);
}
