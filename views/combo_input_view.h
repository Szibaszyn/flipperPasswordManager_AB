#pragma once

#include <gui/view.h>
#include "../vault/vault.h"

typedef struct ComboInputView ComboInputView;

typedef void (*ComboInputCallback)(const uint8_t* arrows, size_t count, void* context);
typedef void (*ComboCancelCallback)(void* context);

ComboInputView* combo_input_view_alloc(void);
void combo_input_view_free(ComboInputView* instance);
View* combo_input_view_get_view(ComboInputView* instance);

void combo_input_view_set_header(ComboInputView* instance, const char* header);
void combo_input_view_set_callback(ComboInputView* instance, ComboInputCallback cb, void* context);
void combo_input_view_set_cancel_callback(ComboInputView* instance, ComboCancelCallback cb, void* context);
void combo_input_view_reset(ComboInputView* instance);
