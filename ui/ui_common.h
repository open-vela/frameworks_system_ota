/*
 * Copyright (C) 2022 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef _UI_COMMON_H_
#define _UI_COMMON_H_

#include <syslog.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define UI_MAX(a, b) ((a) > (b) ? (a) : (b))
#define UI_MIN(a, b) ((a) > (b) ? (b) : (a))

#define FB_DEFAULT_DEV_PATH "/dev/fb0"
#define OTA_UI_DEFAULT_CONFIG_PATH "/resource/recovery/ota_ui_config.json"

#define UI_LOG_DEBUG(format, ...) syslog(LOG_DEBUG, "%s: " format, __FUNCTION__, ##__VA_ARGS__)
#define UI_LOG_INFO(format, ...) syslog(LOG_INFO, " %s: " format, __FUNCTION__, ##__VA_ARGS__)
#define UI_LOG_WARN(format, ...) syslog(LOG_WARNING, "%s: " format, __FUNCTION__, ##__VA_ARGS__)
#define UI_LOG_ERROR(format, ...) syslog(LOG_ERR, "%s: " format, __FUNCTION__, ##__VA_ARGS__)

#define UI_TICK_MS (10 * 1000) /*10ms*/
#define UI_SYNC_OTA_PROGRESS_TICKS (200)

typedef struct ui_area_s ui_area_t;
typedef struct ui_offset_s ui_offset_t;
typedef struct ui_align_s ui_align_t;
typedef struct ui_obj_s ui_obj_t;
typedef struct ui_label_s ui_label_t;
typedef struct progress_map_s progress_map_t;
typedef struct string_map_s string_map_t;
typedef struct ui_progress_s ui_progress_t;
typedef struct ui_ota_page_s ui_ota_page_t;
typedef struct ui_ota_s ui_ota_t;
typedef struct img_head_s img_head_t;
typedef struct upgrade_progress_s upgrade_progress_t;

typedef void (*ui_obj_draw)(ui_obj_t* obj, void* fbHandle);
typedef void (*ui_obj_free)(ui_obj_t* obj);

typedef enum {
    OTA_UI_LOGO = 0,
    OTA_UI_UPGRADE
} ui_mode_e;

typedef enum {
    UI_OBJ_LABEL = 0,
    UI_OBJ_PROGRESS,
    UI_OBJ_PAGE,
    UI_OBJ_INVALID
} ui_obj_type_e;

typedef enum {
    PROGRESS_MODE_NUMBER = 0,
    PROGRESS_MODE_BAR,
    PROGRESS_MODE_INVALID
} ui_progress_mode_e;

typedef enum {
    ALIGN_HOR_LEFT = 0,
    ALIGN_HOR_CENTER,
    ALIGN_HOR_RIGHT
} ui_align_hor_e;

typedef enum {
    ALIGN_VER_TOP = 0,
    ALIGN_VER_CENTER,
    ALIGN_VER_BOTTOM
} ui_align_ver_e;

struct ui_area_s {
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
};

struct ui_offset_s {
    int32_t x_offset;
    int32_t y_offset;
};

struct ui_align_s {
    uint32_t align_hor;
    uint32_t align_ver;
};

struct ui_obj_s {
    ui_area_t area;
    ui_offset_t offset;
    ui_align_t align;
    ui_obj_type_e type;
    ui_ota_page_t* parent_page;
    ui_obj_draw ui_draw;
    ui_obj_free ui_free;
};

struct ui_label_s {
    ui_obj_t obj;
    uint8_t* img_buffer;
};

struct progress_map_s {
    int32_t key;
    uint8_t* img_buffer;
};

struct ui_progress_s {
    ui_obj_t obj;
    int32_t mode;
    int32_t val;
    uint8_t* percentage_img_buffer;
    progress_map_t* img_map;
    int32_t img_map_len;
};

struct ui_ota_page_s {
    ui_area_t dirty_area;
    ui_obj_t** obj_list;
    size_t list_num;
};

struct ui_ota_s {
    uint32_t bg_color;
    ui_ota_page_t logo_page;
    ui_ota_page_t upgrading_page;
    ui_ota_page_t upgrade_fail_page;
    ui_ota_page_t upgrade_success_page;
};

struct string_map_s {
    const char* key_str;
    int32_t value;
};

struct img_head_s {
    uint32_t cf : 5; /* Color format: See `lv_img_color_format_t` */
    uint32_t always_zero : 3; /* It the upper bits of the first byte. Always
                                 zero to look like a non-printable character */
    uint32_t reserved : 2; /* Reserved to be used later */
    uint32_t w : 11; /* Width of the image map    */
    uint32_t h : 11; /* Height of the image map   */
};

struct upgrade_progress_s {
    int32_t cur; /* cur   progress from vela os */
    int32_t next; /* next  progress from vela os */
    uint32_t display; /* local progress  */
};

void ota_ui_set_area(ui_area_t* area, int32_t x, int32_t y, int32_t w, int32_t h);

int32_t ota_ui_config_init(const char* path, ui_ota_t* ui_ota, ui_mode_e mode);
void ota_ui_page_print(ui_ota_page_t* page);
void ota_ui_config_print(ui_ota_t* ui_ota);
void ota_ui_config_destroy(ui_ota_t* ui_ota);

ui_label_t* ui_label_create(ui_ota_page_t* page);
ui_progress_t* ui_progress_create(ui_ota_page_t* page);

#ifdef __cplusplus
}
#endif

#endif
