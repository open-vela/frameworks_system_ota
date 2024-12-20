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

#include <lvgl/lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "extra/lv_upgrade.h"
#include "ui_common.h"

extern int32_t ui_config_init(const char* path);
extern void ui_config_destroy(void);
extern int lv_porting_init(void);

lv_obj_t* ui_find_page(const char* page_name)
{
    int i;
    int page_count = lv_obj_get_child_cnt(lv_scr_act());

    for (i = 0; i < page_count; i++) {
        lv_obj_t* lv_page = lv_obj_get_child(lv_scr_act(), i);
        if (!lv_page || !lv_page->user_data) {
            continue;
        }
        if (strcmp(lv_page->user_data, page_name) == 0) {
            return lv_page;
        }
    }
    return NULL;
}

lv_obj_t* ui_get_upgrade_obj_by_class(lv_obj_t* lv_obj)
{
    int i;
    for (i = 0; i < lv_obj_get_child_cnt(lv_obj); i++) {
        lv_obj_t* obj = lv_obj_get_child(lv_obj, i);
        if (lv_obj_get_class(obj) == &lv_upgrade_class) {
            return obj;
        }
    }
    return NULL;
}

ui_result_code_e ui_refresh_page_progress(lv_obj_t* lv_page, uint32_t value)
{
    if (!lv_page) {
        UI_LOG_ERROR("ota: ui page not found!\n");
        return UI_PAGE_NOT_FOUND;
    }

    lv_obj_t* progress_base_obj = lv_obj_get_child(lv_page, 0);
    if (!progress_base_obj) {
        return UI_LV_OBJ_CHILD_NULL;
    }

    /* get progress obj */
    lv_obj_t* upgrade_obj = ui_get_upgrade_obj_by_class(progress_base_obj);
    if (!upgrade_obj) {
        return UI_LV_OBJ_NULL;
    }

    /* set upgrade progress */
    lv_upgrade_set_progress(upgrade_obj, value);

    return UI_SUCCESS;
}

ui_result_code_e ui_init()
{
    lv_nuttx_dsc_t info;
    lv_nuttx_result_t result;

    lv_init();

    lv_nuttx_dsc_init(&info);
    lv_nuttx_init(&info, &result);

    return UI_SUCCESS;
}

ui_result_code_e ui_config_parse(const char* file_path)
{
    int ret = ui_config_init(file_path);
    if (ret < 0) {
        UI_LOG_ERROR("ota: ui page config init error, code:%d \n", ret);
        return UI_FILE_PARSE_ERROR;
    }

    return UI_SUCCESS;
}

ui_result_code_e ui_page_show(const char* page_name)
{
    int i = 0;
    int page_count = 0;

    lv_obj_t* lv_page = ui_find_page(page_name);
    if (!lv_page) {
        UI_LOG_ERROR("ota: ui page not found!\n");
        return UI_PAGE_NOT_FOUND;
    }

    if (lv_obj_is_visible(lv_page)) {
        return UI_SUCCESS;
    }

    /* show the page */
    lv_obj_clear_flag(lv_page, LV_OBJ_FLAG_HIDDEN);

    /* hide other pages */
    page_count = lv_obj_get_child_cnt(lv_scr_act());
    for (i = 0; i < page_count; i++) {
        lv_obj_t* page = lv_obj_get_child(lv_scr_act(), i);
        if (!page) {
            continue;
        }
        if (page != lv_page) {
            lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
        }
    }

    return UI_SUCCESS;
}

ui_result_code_e ui_page_hide(const char* page_name)
{
    lv_obj_t* lv_page = ui_find_page(page_name);
    if (!lv_page) {
        UI_LOG_ERROR("ota: ui page not found!\n");
        return UI_PAGE_NOT_FOUND;
    }

    if (lv_obj_is_visible(lv_page)) {
        lv_obj_add_flag(lv_page, LV_OBJ_FLAG_HIDDEN);
    }

    return UI_SUCCESS;
}

ui_result_code_e ui_set_progress(const char* page_name, uint32_t value)
{
    if (!page_name) {
        UI_LOG_ERROR("ota: ui page name null!\n");
        return UI_PAGE_NAME_NULL;
    }

    /* find page */
    lv_obj_t* lv_page = ui_find_page(page_name);
    if (!lv_page) {
        UI_LOG_ERROR("ota: ui page not found!\n");
        return UI_PAGE_NOT_FOUND;
    }

    return ui_refresh_page_progress(lv_page, value);
}

bool ui_page_exist(const char* page_name)
{
    if (!page_name) {
        UI_LOG_ERROR("ota: ui page name null!\n");
        return false;
    }

    lv_obj_t* lv_page = ui_find_page(page_name);
    if (!lv_page) {
        return false;
    }
    return true;
}

const char* ui_get_current_page_name(void)
{
    int i;
    int page_count = lv_obj_get_child_cnt(lv_scr_act());

    for (i = 0; i < page_count; i++) {
        lv_obj_t* lv_page = lv_obj_get_child(lv_scr_act(), i);
        if (!lv_page) {
            continue;
        }
        if (lv_obj_is_visible(lv_page)) {
            return lv_page->user_data;
        }
    }
    return NULL;
}

ui_result_code_e ui_timer_handler()
{
    uint32_t idle;
    idle = lv_timer_handler();

    /* Minimum sleep of 1ms */

    idle = idle ? idle : 1;
    usleep(idle * 1000);

    return UI_SUCCESS;
}

void ui_uninit()
{
    ui_config_destroy();
}