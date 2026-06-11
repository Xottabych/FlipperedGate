#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/submenu.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <storage/storage.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <furi_hal_rtc.h>
#include <furi_hal_cortex.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define CAPTURE_BUFFER_SIZE 512
#define SUBGHZ_OUTPUT_DIR   "/ext/subghz"
#define RECENT_FILES_MAX    16

typedef enum { ScanModeSweep = 0, ScanModeFixed = 1 } ScanMode;
typedef enum { ModOOK = 0, ModFSK = 1 } ModulationMode;

typedef enum {
    AppViewMainMenu,
    AppViewScanning,
    AppViewSuccess,
    AppViewSettings,
    AppViewRecent,
    AppViewDecode,
    AppViewCount,
} AppView;

typedef enum {
    AppCustomEventScanTick,
    AppCustomEventSignalFound,
    AppCustomEventCaptureDone,
    AppCustomEventSaveOk,
    AppCustomEventSaveError,
} AppCustomEvent;

typedef enum {
    FreqRange_All,
    FreqRange_433,
    FreqRange_315,
} FreqRange;

typedef enum {
    AntennaExternal = 0, /* SPI bit-bang CC1101 on GPIO header */
    AntennaInternal = 1, /* Built-in Flipper CC1101 via furi_hal_subghz */
} AntennaMode;

typedef struct {
    /* GPIO pins for external CC1101 via SPI bit-bang */
    const GpioPin* pin_mosi; /* Pin 2 PA7 */
    const GpioPin* pin_miso; /* Pin 3 PA6 */
    const GpioPin* pin_cs;   /* Pin 4 PA4 */
    const GpioPin* pin_sck;  /* Pin 5 PB3 */
    const GpioPin* pin_gd0;  /* Pin 6 PB2 */

    bool cc1101_present;

    /* GUI framework objects */
    Gui*             gui;
    ViewDispatcher*  view_dispatcher;
    SceneManager*    scene_manager;

    /* Views */
    View*             view_main_menu;
    View*             view_scanning;
    View*             view_success;
    VariableItemList* view_settings;
    Submenu*          view_recent;
    Submenu*          view_decode;

    /* Scanner state */
    uint8_t  freq_index;
    float    current_freq_mhz;
    int8_t   current_rssi;
    int8_t   rssi_threshold;
    uint32_t dwell_start_tick;
    FuriTimer* scan_timer;

    /* Signal capture */
    int32_t*  capture_buf;
    uint16_t  capture_len;
    uint32_t  capture_freq_hz;
    volatile bool capturing;
    volatile bool capture_done_flag;
    bool          capture_rx_started;  /* true while furi_hal_subghz_start_async_rx is active */
    bool          gdo0_int_registered; /* true while the GDO0 edge interrupt is installed */
    bool          otg_enabled;          /* true while furi_hal_power_enable_otg has been called */
    bool          subghz_started;       /* true while furi_hal_subghz has been initialised */
    bool          subghz_in_rx;         /* true while radio is in Rx state (not Idle/AsyncRx) */
    uint32_t      cpu_mhz;             /* cached CPU clock = 64 for STM32WB55 */
    uint32_t  last_capture_hash;
    uint32_t  last_capture_ts;
    uint32_t  last_edge_tick;
    bool      edge_level;

    /* Settings */
    int8_t          squelch_threshold;
    FreqRange       freq_range;
    AntennaMode     antenna_mode;   /* default = AntennaExternal */
    uint32_t        dwell_ms;
    ScanMode        scan_mode;
    uint32_t        fixed_freq_hz;
    ModulationMode  modulation;

    /* Notifications */
    NotificationApp* notifications;

    /* Recent files list */
    char    recent_files[RECENT_FILES_MAX][64];
    uint8_t recent_files_count;

    /* Save result path */
    char save_path[128];
} AppState;
