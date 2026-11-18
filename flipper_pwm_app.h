#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <storage/storage.h>
#include "vault/vault.h"
#include "views/combo_input_view.h"
#include "usb/mass_storage_usb.h"

#define PWM_APP_DATA_DIR    STORAGE_APP_DATA_PATH_PREFIX
#define PWM_VAULT_FILE_PATH STORAGE_APP_DATA_PATH_PREFIX "/vault.dat"
#define PWM_DISK_IMG_PATH   STORAGE_APP_DATA_PATH_PREFIX "/passwords.img"

typedef enum {
    FlipperPwmViewSubmenu,
    FlipperPwmViewWidget,
    FlipperPwmViewComboInput,
} FlipperPwmView;

typedef enum {
    FlipperPwmSceneMainMenu,
    FlipperPwmSceneComboInput,
    FlipperPwmSceneSaveMode,
    FlipperPwmSceneEnterList,
    FlipperPwmSceneTypePassword,
    FlipperPwmSceneHelp,
    FlipperPwmSceneNum,
} FlipperPwmScene;

typedef enum {
    ComboModeUnlockSave,
    ComboModeUnlockEnter,
    ComboModeChangeCombo,
} ComboInputMode;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Submenu* submenu;
    Widget* widget;
    ComboInputView* combo_view;

    PasswordVault vault;
    uint8_t derived_key[32];
    bool vault_unlocked;
    ComboInputMode combo_mode;
    uint32_t selected_entry_idx;

    File* disk_file;
    MassStorageUsb* usb_msc;
} FlipperPwmApp;

enum FlipperPwmCustomEvent {
    PwmEventSaveModeDone = 100,
    PwmEventSaveModeCancel,
    PwmEventTypeDone,
};
