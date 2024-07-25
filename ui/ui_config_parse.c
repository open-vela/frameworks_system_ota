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

#include <assert.h>
#include <cJSON.h>
#include <fcntl.h>
#include <lvgl/lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "extra/lv_upgrade.h"
#include "ui_common.h"

typedef struct string_map_s {
    const char* key_str;
    int32_t value;
} string_map_t;

static void ui_event_handler(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* obj = lv_event_get_target(e);
    if (!obj) {
        return;
    }

    if (code == LV_EVENT_DELETE) {
        if (obj->user_data) {
            free(obj->user_data);
            obj->user_data = NULL;
        }
    }
}

static int32_t string_2_id(const char* key, const string_map_t* map, int map_len, int default_val)
{
    int32_t i = 0, ret = default_val;
    const string_map_t* node = map;
    if (!key || !map)
        return ret;

    for (i = 0; i < map_len; i++) {
        if (strcmp(node->key_str, key) == 0) {
            ret = node->value;
            break;
        }
        node++;
    }
    return ret;
}

static lv_align_t number_2_align(const int horNum, const int verKey)
{
    int temp_val = horNum | (verKey << 2);
    switch (temp_val) {
    case 0:
        return LV_ALIGN_TOP_LEFT;
    case 1:
        return LV_ALIGN_TOP_MID;
    case 2:
        return LV_ALIGN_TOP_RIGHT;
    case 4:
        return LV_ALIGN_LEFT_MID;
    case 5:
        return LV_ALIGN_CENTER;
    case 6:
        return LV_ALIGN_RIGHT_MID;
    case 8:
        return LV_ALIGN_BOTTOM_LEFT;
    case 9:
        return LV_ALIGN_BOTTOM_MID;
    case 10:
        return LV_ALIGN_BOTTOM_RIGHT;
    default:
        break;
    }
    return LV_ALIGN_DEFAULT;
}

static uint8_t* read_all_from_file(const char* path, uint32_t* data_size)
{
    int fd = 0;
    size_t flen = 0;
    uint8_t* content = NULL;

    fd = open(path, O_RDONLY);

    if (fd < 0) {
        UI_LOG_WARN("ui file :%s open failed!\n", path);
        return NULL;
    }

    flen = lseek(fd, 0L, SEEK_END);
    lseek(fd, 0L, SEEK_SET);

    content = malloc(flen + 1);
    if (data_size) {
        *data_size = flen + 1;
    }

    DEBUGASSERT(content != NULL);

    if (content) {
        read(fd, content, flen);
        content[flen] = 0;
    }
    close(fd);
    return content;
}

static uint8_t* get_image_data_from_file(const char* path)
{
    uint32_t data_size = 0;
    uint8_t* image_buff = read_all_from_file(path, &data_size);
    if (!image_buff) {
        UI_LOG_ERROR("ui read image file error.\n");
        return NULL;
    }

    lv_img_dsc_t* dsc = (lv_image_dsc_t*)image_buff;
    dsc->data_size = (uint32_t)(data_size - sizeof(lv_image_header_t));
    dsc->data = image_buff + sizeof(lv_image_header_t);

    return image_buff;
}

static int32_t json_area_parse(cJSON* node, lv_obj_t* lv_obj)
{
    cJSON* x = cJSON_GetObjectItemCaseSensitive(node, "x");
    cJSON* y = cJSON_GetObjectItemCaseSensitive(node, "y");
    cJSON* w = cJSON_GetObjectItemCaseSensitive(node, "w");
    cJSON* h = cJSON_GetObjectItemCaseSensitive(node, "h");

    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(w) || !cJSON_IsNumber(h)) {
        return UI_FILE_CONTENT_INVALID;
    }

    lv_obj_t* parent_obj = lv_obj_get_parent(lv_obj);
    if (parent_obj) {
        lv_obj_set_pos(parent_obj, x->valueint, y->valueint);
        lv_obj_set_size(parent_obj, w->valueint, h->valueint);
    }

    return UI_SUCCESS;
}

static int32_t json_offset_parse(cJSON* node, lv_obj_t* lv_obj)
{
    cJSON* x = cJSON_GetObjectItemCaseSensitive(node, "x");
    cJSON* y = cJSON_GetObjectItemCaseSensitive(node, "y");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y)) {
        return UI_FILE_CONTENT_INVALID;
    }

    lv_obj_set_pos(lv_obj, x->valueint, y->valueint);

    return UI_SUCCESS;
}

static int32_t json_align_parse(cJSON* node, lv_obj_t* lv_obj)
{
    int tmp_hor_align = 0;
    int tmp_ver_align = 0;
    int align = 0;
    cJSON *hor_align = NULL, *ver_align = NULL;

    static const string_map_t hor_align_map[] = {
        { "left", 0 },
        { "center", 1 },
        { "right", 2 }
    };

    static const string_map_t ver_align_map[] = {
        { "top", 0 },
        { "center", 1 },
        { "bottom", 2 }
    };

    if (!cJSON_IsArray(node)) {
        return UI_FILE_CONTENT_INVALID;
    }

    /* json align must contain both vertical and horizonal layout */
    if (cJSON_GetArraySize(node) != 2) {
        return UI_FILE_CONTENT_INVALID;
    }

    hor_align = cJSON_GetArrayItem(node, 0);
    ver_align = cJSON_GetArrayItem(node, 1);
    if (!cJSON_IsString(hor_align) || !cJSON_IsString(ver_align)) {
        return UI_FILE_CONTENT_INVALID;
    }

    tmp_hor_align = string_2_id(hor_align->valuestring, hor_align_map, ARRAY_SIZE(hor_align_map), 0);
    tmp_ver_align = string_2_id(ver_align->valuestring, ver_align_map, ARRAY_SIZE(ver_align_map), 0);

    align = number_2_align(tmp_hor_align, tmp_ver_align);

    lv_obj_align(lv_obj, align, 0, 0);

    return UI_SUCCESS;
}

static int32_t json_ui_image_set(lv_obj_t* lv_obj, const char* image_path)
{
    uint8_t* image_buff = get_image_data_from_file(image_path);
    if (!image_buff) {
        UI_LOG_ERROR("ui read image file error.\n");
        return UI_FILE_READ_ERROR;
    }

    lv_image_set_src(lv_obj, (lv_image_dsc_t*)image_buff);
    lv_image_set_offset_y(lv_obj, -1);

    lv_obj->user_data = image_buff;

    return UI_SUCCESS;
}

static int32_t json_ui_obj_parse(cJSON* jsonObj, lv_obj_t* lv_obj)
{
    int ret = 0;
    cJSON* area = NULL;
    cJSON* align = NULL;
    cJSON* offset = NULL;

    /* align is an optional attr */
    align = cJSON_GetObjectItem(jsonObj, "align");
    if (json_align_parse(align, lv_obj) < 0) {
        UI_LOG_WARN("ui no align information, use default align value.\n");
    }

    area = cJSON_GetObjectItem(jsonObj, "area");
    ret = json_area_parse(area, lv_obj);
    if (ret < 0) {
        UI_LOG_ERROR("ui parse area config error, code: %d\n", ret);
        return UI_FILE_PARSE_ERROR;
    }

    /* offset is an optional attr */
    offset = cJSON_GetObjectItem(jsonObj, "offset");
    if (json_offset_parse(offset, lv_obj) < 0) {
        UI_LOG_WARN("ui no offset information, ignore it.\n");
    }

    return UI_SUCCESS;
}

static int32_t json_progress_parse(cJSON* progress, lv_obj_t* page)
{
    int i = 0;
    int mode_type = 0;
    cJSON* progress_img_src_map = NULL;
    cJSON* progress_mode = NULL;
    cJSON* animation_fps = NULL;
    cJSON* animation_radius = NULL;
    cJSON* percentage_img = NULL;
    cJSON* a_element = NULL;
    uint32_t img_map_len = 0;
    lv_obj_t* upgrade_obj = NULL;
    lv_obj_t* lv_progress_base_obj = NULL;

    static const string_map_t progress_mode_map[] = {
        { "number", PROGRESS_MODE_NUMBER },
        { "bar", PROGRESS_MODE_BAR },
        { "circle", PROGRESS_MODE_CIRCLE },
        { "animation", PROGRESS_MODE_ANIM },
        { "custom_anim", PROGRESS_MODE_CUSTOM_ANIM }
    };

    if (!page) {
        UI_LOG_ERROR("ui parse object null.\n");
        return UI_PAGE_NOT_FOUND;
    }

    /* create lvgl progress obj */
    lv_progress_base_obj = lv_obj_create(page);
    lv_obj_remove_style(lv_progress_base_obj, NULL, LV_PART_ANY | LV_STATE_ANY);

    /* create upgrade obj */
    upgrade_obj = lv_upgrade_create(lv_progress_base_obj);

    if (json_ui_obj_parse(progress, upgrade_obj) < 0) {
        UI_LOG_ERROR("ui parse ui obj config error.\n");
        return UI_FILE_PARSE_ERROR;
    }

    progress_mode = cJSON_GetObjectItem(progress, "mode");
    if (!cJSON_IsString(progress_mode)) {
        UI_LOG_ERROR("ui parse progress mode config error.\n");
        return UI_FILE_PARSE_ERROR;
    }

    mode_type = string_2_id(progress_mode->valuestring, progress_mode_map, ARRAY_SIZE(progress_mode_map), PROGRESS_MODE_INVALID);
    if (mode_type == PROGRESS_MODE_INVALID) {
        UI_LOG_ERROR("ui parse progress mode config error. val : %s\n", progress_mode->valuestring);
        return UI_FILE_CONTENT_INVALID;
    }

    lv_upgrade_set_type(upgrade_obj, (lv_upgrade_type_e)mode_type);

    animation_fps = cJSON_GetObjectItem(progress, "animation_fps");
    if (cJSON_IsNumber(animation_fps)) {
        lv_upgrade_set_animation_fps(upgrade_obj, animation_fps->valueint);
    }

    animation_radius = cJSON_GetObjectItem(progress, "animation_radius");
    if (cJSON_IsNumber(animation_radius)) {
        lv_upgrade_set_animation_radius(upgrade_obj, animation_radius->valueint);
    }

    /* optional attr */
    percentage_img = cJSON_GetObjectItem(progress, "percentage_src");
    if (cJSON_IsString(percentage_img)) {
        lv_upgrade_set_percent_sign_image(upgrade_obj, percentage_img->valuestring);
    }

    progress_img_src_map = cJSON_GetObjectItem(progress, "img_src_list");
    if ((img_map_len = cJSON_GetArraySize(progress_img_src_map)) <= 0) {
        UI_LOG_INFO("ui parse progress image list null.\n");
    }

    if (mode_type == PROGRESS_MODE_NUMBER && img_map_len < 10) {
        UI_LOG_ERROR("ui img list length should be 10 [0.1.2.3.4.5.6.7.8.9] in number mode.\n");
        return UI_FILE_CONTENT_INVALID;
    }

    /* cache img list in prog obj user_data */
    lv_upgrade_set_image_array_size(upgrade_obj, img_map_len);
    cJSON_ArrayForEach(a_element, progress_img_src_map)
    {
        int key = (int)strtol(a_element->string, NULL, 10);
        if (mode_type == PROGRESS_MODE_NUMBER && key != i) {
            UI_LOG_ERROR("ui img list length should be the order like [0.1.2.3.4.5.6.7.8.9] in number mode.\n");
            break;
        }
        /* read file to map */
        lv_upgrade_set_image_data(upgrade_obj, key, a_element->valuestring);
        i++;
    }

    return UI_SUCCESS;
}

static int32_t json_label_parse(cJSON* label, lv_obj_t* page)
{
    cJSON* labelImgSrc = NULL;

    /* create lvgl label obj */
    lv_obj_t* label_obj = lv_obj_create(page);
    lv_obj_remove_style(label_obj, NULL, LV_PART_ANY | LV_STATE_ANY);

    /* create img obj */
    lv_obj_t* img_label = lv_img_create(label_obj);
    lv_obj_add_event(img_label, ui_event_handler, LV_EVENT_ALL, NULL);

    if (json_ui_obj_parse(label, img_label) < 0) {
        UI_LOG_ERROR("ui parse ui obj config error.\n");
        return UI_FILE_PARSE_ERROR;
    }

    labelImgSrc = cJSON_GetObjectItem(label, "img_src");
    if (!cJSON_IsString(labelImgSrc)) {
        UI_LOG_ERROR("ui parse image src config error.\n");
        return UI_FILE_PARSE_ERROR;
    }

    if (json_ui_image_set(img_label, labelImgSrc->valuestring) < 0) {
        UI_LOG_ERROR("ui ui obj set image source error.\n");
        return UI_FILE_PARSE_ERROR;
    }

    return UI_SUCCESS;
}

static int32_t json_ui_page_cont_parse(cJSON* ui_obj, lv_obj_t* page_obj)
{
    int ret = 0;
    if (!cJSON_IsObject(ui_obj)) {
        UI_LOG_ERROR("ui parse object invaild!\n");
        return UI_FILE_CONTENT_INVALID;
    }

    if (strncmp(ui_obj->string, "progress", strlen("progress")) == 0) {
        /* parse progress obj */
        ret = json_progress_parse(ui_obj, page_obj);
        if (ret < 0) {
            UI_LOG_ERROR("ui parse progress config error. code:%d\n", ret);
            return UI_FILE_PARSE_ERROR;
        }
    } else if (strncmp(ui_obj->string, "label", strlen("label")) == 0) {
        /* parse label obj */
        ret = json_label_parse(ui_obj, page_obj);
        if (ret < 0) {
            UI_LOG_ERROR("ui parse label config error. code: %d\n", ret);
            return UI_FILE_PARSE_ERROR;
        }
    } else {
        UI_LOG_WARN("ui Unknown UI obj type:%s.\n", ui_obj->string);
        return UI_FILE_CONTENT_INVALID;
    }

    return UI_SUCCESS;
}

static void* create_page_obj(uint32_t bgcolor)
{
    // create object
    lv_obj_t* obj = lv_obj_create(lv_scr_act());
    lv_obj_remove_style(obj, NULL, LV_PART_ANY | LV_STATE_ANY);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bgcolor), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);

    lv_obj_set_size(obj, LV_HOR_RES, LV_VER_RES);

    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);

    return (void*)obj;
}

static lv_obj_t* json_ui_page_parse(cJSON* page, uint32_t bgcolor)
{
    int i = 0;
    cJSON* ele = NULL;

    if (!page) {
        UI_LOG_ERROR("ui parse page error!\n");
        return NULL;
    }

    /* create lvgl obj */
    lv_obj_t* lv_obj = create_page_obj(bgcolor);

    /* init lv_obj page name */
    lv_obj->user_data = strdup(page->string);

    /* parse json */
    int json_array_size = (size_t)cJSON_GetArraySize(page);
    cJSON_ArrayForEach(ele, page)
    {
        if (json_ui_page_cont_parse(ele, lv_obj) < 0) {
            UI_LOG_ERROR("ui page config parse obj(%d) error!\n", i);
            break;
        }
        i++;
    }
    if (i != json_array_size) {
        lv_obj_clean(lv_scr_act());
        return NULL;
    }

    return lv_obj;
}

static int32_t ui_page_config_init(cJSON* root)
{
    uint32_t bg_color = 0;
    cJSON* ele = NULL;

    if (!root) {
        UI_LOG_ERROR("ui json file parse error!\n");
        return UI_FILE_PARSE_ERROR;
    }

    cJSON_bool has_bg_color = cJSON_HasObjectItem(root, "bg_color");
    if (has_bg_color) {
        /* background color parser, optional attr */
        cJSON* bg_color_json = cJSON_GetObjectItem(root, "bg_color");
        if (cJSON_IsString(bg_color_json)) {
            bg_color = (uint32_t)strtoul(bg_color_json->valuestring, NULL, 16);
        } else {
            UI_LOG_INFO("ui use default background color!\n");
        }
    }

    cJSON_ArrayForEach(ele, root)
    {
        if (strcmp(ele->string, "bg_color") == 0) {
            continue;
        }
        if (json_ui_page_parse(ele, bg_color) == NULL) {
            UI_LOG_ERROR("ui config parse page(%s) error!\n", ele->string);
            break;
        }
    }

    return UI_SUCCESS;
}

int32_t ui_config_init(const char* path)
{
    uint8_t* json_buf = NULL;
    cJSON* root = NULL;

    if (!path) {
        UI_LOG_ERROR("ui load ui config file path null.\n");
        return UI_FILE_PATH_NULL;
    }

    json_buf = read_all_from_file(path, NULL);
    if (!json_buf) {
        UI_LOG_ERROR("ui load ui config error, file path:%s\n", path);
        return UI_FILE_READ_ERROR;
    }

    root = cJSON_Parse((const char*)json_buf);
    if (!root) {
        free(json_buf);
        return UI_FILE_PARSE_ERROR;
    }

    if (ui_page_config_init(root) < 0) {
        UI_LOG_ERROR("ui load pages config error!\n");
        return UI_FILE_PARSE_ERROR;
    }

    return UI_SUCCESS;
}

void ui_config_destroy(void)
{
    lv_obj_clean(lv_scr_act());
}
