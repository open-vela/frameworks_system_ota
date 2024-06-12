/*
 * Copyright (C) 2024 Xiaomi Corporation
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

#define UI_LOG_DEBUG(format, ...) syslog(LOG_DEBUG, "%s: " format, __FUNCTION__, ##__VA_ARGS__)
#define UI_LOG_INFO(format, ...) syslog(LOG_INFO, "%s: " format, __FUNCTION__, ##__VA_ARGS__)
#define UI_LOG_WARN(format, ...) syslog(LOG_WARNING, "%s: " format, __FUNCTION__, ##__VA_ARGS__)
#define UI_LOG_ERROR(format, ...) syslog(LOG_ERR, "%s: " format, __FUNCTION__, ##__VA_ARGS__)

#define UI_TICK_MS (10 * 1000) /*10ms*/

typedef enum {
    UI_SUCCESS = 0,
    UI_FILE_PARSE_ERROR = -1,
    UI_PAGE_NAME_NULL = -2,
    UI_PAGE_NOT_FOUND = -3,
    UI_USER_DATA_NULL = -4,
    UI_PAGE_DATA_NULL = -5,
    UI_FILE_PATH_NULL = -6,
    UI_LV_OBJ_NULL = -7,
    UI_LV_OBJ_CHILD_NULL = -8,
    UI_IMAGE_BUFF_NULL = -9,
    UI_FILE_READ_ERROR = -10,
    UI_FILE_CONTENT_INVALID = -11
} ui_result_code_e;

typedef enum {
    PROGRESS_MODE_NUMBER = 0,
    PROGRESS_MODE_BAR,
    PROGRESS_MODE_CIRCLE,
    PROGRESS_MODE_ANIM,
    PROGRESS_MODE_CUSTOM_ANIM,
    PROGRESS_MODE_INVALID
} ui_progress_mode_e;

/**
 * Init lvgl porting
 * @param void
 * @return return code ui_result_code_e
 */
ui_result_code_e ui_init(void);

/**
 * Parse config file
 * @param page_name pointer to config file path
 * @return code of ui_result_code_e
 */
ui_result_code_e ui_config_parse(const char* file_path);

/**
 * Show page
 * @param page_name pointer to page name
 * @return code of ui_result_code_e
 */
ui_result_code_e ui_page_show(const char* page_name);

/**
 * Hide page
 * @param page_name pointer to page name
 * @return code of ui_result_code_e
 */
ui_result_code_e ui_page_hide(const char* page_name);

/**
 * Set and update display progress
 * @param page_name pointer to page width progress comp
 * @param value proggress (0 ~ 100)
 * @return code of ui_result_code_e
 */
ui_result_code_e ui_set_progress(const char* page_name, uint32_t value);

/**
 * Get page is exist
 * @param page_name pointer to page name
 * @return page exist or not
 */
bool ui_page_exist(const char* page_name);

/**
 * Get current show page name
 * @param void
 * @return pointer to current page name
 */
const char* ui_get_current_page_name(void);

/**
 * Handler looper
 * @param void
 * @return return code ui_result_code_e
 */
ui_result_code_e ui_timer_handler(void);

/**
 * Uninit object of lvgl
 * @param void
 * @return void
 */
void ui_uninit(void);

#ifdef __cplusplus
}
#endif

#endif
