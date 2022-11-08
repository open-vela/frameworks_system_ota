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

#include <cJSON.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_common.h"

static void ota_page_config_destroy(ui_ota_page_t* ota_page);

static int32_t string_2_id(const char* key, const string_map_t* map, int map_len)
{
    int32_t i = 0, ret = -1;
    const string_map_t* node = map;
    if (!key || !map)
        return -1;

    for (i = 0; i < map_len; i++) {
        if (strcmp(node->key_str, key) == 0) {
            ret = node->value;
            break;
        }
        node++;
    }
    return ret;
}

static uint8_t* read_all_from_file(const char* path)
{
    int fd = 0;
    size_t flen = 0;
    uint8_t* content = NULL;

    fd = open(path, O_RDONLY);

    if (fd < 0) {
        UI_LOG_WARN("file :%s open failed!\n", path);
        return NULL;
    }

    flen = lseek(fd, 0L, SEEK_END);
    lseek(fd, 0L, SEEK_SET);

    content = malloc(flen + 1);

    DEBUGASSERT(content != NULL);

    if (content) {
        read(fd, content, flen);
        content[flen] = 0;
    }
    close(fd);
    return content;
}

static int32_t json_area_parse(cJSON* node, ui_area_t* ui_area)
{
    cJSON* x = cJSON_GetObjectItemCaseSensitive(node, "x");
    cJSON* y = cJSON_GetObjectItemCaseSensitive(node, "y");
    cJSON* w = cJSON_GetObjectItemCaseSensitive(node, "w");
    cJSON* h = cJSON_GetObjectItemCaseSensitive(node, "h");

    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(w) || !cJSON_IsNumber(h)) {
        return -1;
    }

    ota_ui_set_area(ui_area, x->valueint, y->valueint, w->valueint, h->valueint);
    return 0;
}

static int32_t json_offset_parse(cJSON* node, ui_offset_t* uiOffset)
{
    cJSON* x = cJSON_GetObjectItemCaseSensitive(node, "x");
    cJSON* y = cJSON_GetObjectItemCaseSensitive(node, "y");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y)) {
        return -1;
    }
    uiOffset->x_offset = x->valueint;
    uiOffset->y_offset = y->valueint;
    return 0;
}

static int32_t json_align_parse(cJSON* node, ui_align_t* ui_align)
{
    cJSON *hor_align = NULL, *ver_align = NULL;

    static const string_map_t hor_align_map[] = {
        { "left", ALIGN_HOR_LEFT },
        { "center", ALIGN_HOR_CENTER },
        { "right", ALIGN_HOR_RIGHT }
    };

    static const string_map_t ver_align_map[] = {
        { "top", ALIGN_VER_TOP },
        { "center", ALIGN_VER_CENTER },
        { "bottom", ALIGN_VER_BOTTOM }
    };

    if (!cJSON_IsArray(node))
        return -1;
    if (cJSON_GetArraySize(node) != 2)
        return -1;

    hor_align = cJSON_GetArrayItem(node, 0);
    ver_align = cJSON_GetArrayItem(node, 1);
    if (!cJSON_IsString(hor_align) || !cJSON_IsString(ver_align)) {
        return -1;
    }

    ui_align->align_hor = string_2_id(hor_align->valuestring, hor_align_map, ARRAY_SIZE(hor_align_map));
    ui_align->align_ver = string_2_id(ver_align->valuestring, ver_align_map, ARRAY_SIZE(ver_align_map));

    return 0;
}

static int32_t json_ui_obj_parse(cJSON* jsonObj, ui_obj_t* ui_obj)
{
    cJSON* area = NULL;
    cJSON* align = NULL;
    cJSON* offset = NULL;

    area = cJSON_GetObjectItem(jsonObj, "area");
    if (json_area_parse(area, &(ui_obj->area)) < 0) {
        UI_LOG_ERROR("parse area config error.\n");
        return -1;
    }

    align = cJSON_GetObjectItem(jsonObj, "align");
    if (json_align_parse(align, &(ui_obj->align)) < 0) {
        UI_LOG_ERROR("parse align config error.\n");
        return -1;
    }

    /* offset is an option attr */
    offset = cJSON_GetObjectItem(jsonObj, "offset");
    if (json_offset_parse(offset, &(ui_obj->offset)) < 0) {
        UI_LOG_INFO("No offset information, ignore it.\n");
    }

    return 0;
}

static int32_t json_progress_parse(cJSON* progress, ui_progress_t* ui_progress)
{
    int32_t i = 0;
    cJSON* progress_img_src_map = NULL;
    cJSON* progress_mode = NULL;
    cJSON* percentage_img = NULL;
    cJSON* a_element = NULL;

    static const string_map_t progress_mode_map[] = {
        { "number", PROGRESS_MODE_NUMBER },
        { "bar", PROGRESS_MODE_BAR }
    };

    if (json_ui_obj_parse(progress, &(ui_progress->obj)) < 0) {
        UI_LOG_ERROR("parse ui obj config error.\n");
        return -1;
    }

    progress_mode = cJSON_GetObjectItem(progress, "mode");
    if (!cJSON_IsString(progress_mode)) {
        UI_LOG_ERROR("parse progress mode config error.\n");
        return -1;
    }
    ui_progress->mode = string_2_id(progress_mode->valuestring, progress_mode_map, ARRAY_SIZE(progress_mode_map));

    /* optional attr */
    percentage_img = cJSON_GetObjectItem(progress, "percentage_src");
    if (cJSON_IsString(percentage_img)) {
        ui_progress->percentage_img_buffer = read_all_from_file(percentage_img->valuestring);
    }

    progress_img_src_map = cJSON_GetObjectItem(progress, "img_src_list");
    if ((ui_progress->img_map_len = cJSON_GetArraySize(progress_img_src_map)) <= 0) {
        UI_LOG_ERROR("parse progress image src config error.\n");
        return -1;
    }

    if (ui_progress->mode == PROGRESS_MODE_NUMBER && ui_progress->img_map_len != 10) {
        UI_LOG_ERROR("img list length should be 10 [0.1.2.3.4.5.6.7.8.9] in number mode.\n");
        return -1;
    }

    ui_progress->img_map = (progress_map_t*)calloc((size_t)ui_progress->img_map_len, sizeof(progress_map_t));

    if (!ui_progress->img_map) {
        return -1;
    }

    cJSON_ArrayForEach(a_element, progress_img_src_map)
    {
        ui_progress->img_map[i].key = (int)strtol(a_element->string, NULL, 10);
        if (ui_progress->mode == PROGRESS_MODE_NUMBER && ui_progress->img_map[i].key != i) {
            UI_LOG_ERROR("img list length should be the order like [0.1.2.3.4.5.6.7.8.9] in number mode.\n");
            break;
        }
        ui_progress->img_map[i].img_buffer = read_all_from_file(a_element->valuestring);
        i++;
    }

    return 0;
}

static int32_t json_label_parse(cJSON* label, ui_label_t* ui_label)
{
    cJSON* labelImgSrc = NULL;

    if (json_ui_obj_parse(label, &(ui_label->obj)) < 0) {
        UI_LOG_ERROR("parse ui obj config error.\n");
        return -1;
    }

    labelImgSrc = cJSON_GetObjectItem(label, "img_src");
    if (!cJSON_IsString(labelImgSrc)) {
        UI_LOG_ERROR("parse image src config error.\n");
        return -1;
    }
    ui_label->img_buffer = read_all_from_file(labelImgSrc->valuestring);
    return 0;
}

static ui_obj_t* json_ota_ui_obj_parse(cJSON* ui_obj, ui_ota_page_t* ota_page)
{
    if (!cJSON_IsObject(ui_obj)) {
        return NULL;
    }

    if (strncmp(ui_obj->string, "progress", strlen("progress")) == 0) {
        ui_progress_t* new_progress = ui_progress_create(ota_page);
        if (new_progress) {
            if (json_progress_parse(ui_obj, new_progress) < 0) {
                UI_LOG_ERROR("parse progress config error.\n");
                free(new_progress);
            } else {
                return (ui_obj_t*)new_progress;
            }
        }
    } else if (strncmp(ui_obj->string, "label", strlen("label")) == 0) {
        ui_label_t* new_label = ui_label_create(ota_page);
        if (new_label) {
            if (json_label_parse(ui_obj, new_label) < 0) {
                UI_LOG_ERROR("parse label config error.\n");
                free(new_label);
            } else {
                return (ui_obj_t*)new_label;
            }
        }
    } else {
        UI_LOG_WARN("Unknown UI obj type:%s.\n", ui_obj->string);
    }
    return NULL;
}

static int32_t ota_page_config_init(cJSON* page, ui_ota_page_t* ota_page)
{
    int32_t i = 0;
    cJSON* ele = NULL;
    if (!page || !ota_page)
        return -1;

    ota_page->list_num = (size_t)cJSON_GetArraySize(page);
    ota_page->obj_list = (ui_obj_t**)calloc(ota_page->list_num, sizeof(ui_obj_t*));

    cJSON_ArrayForEach(ele, page)
    {
        if ((ota_page->obj_list[i] = json_ota_ui_obj_parse(ele, ota_page)) == NULL) {
            UI_LOG_ERROR("page config parse obj(%d) error!\n", i);
            break;
        }
        i++;
    }
    if (i != ota_page->list_num) {
        ota_page_config_destroy(ota_page);
        return -1;
    }

    return 0;
}

static void ota_page_config_destroy(ui_ota_page_t* ota_page)
{
    int32_t i = 0;
    if (!ota_page || !ota_page->obj_list)
        return;

    for (i = 0; i < ota_page->list_num; i++) {
        if (ota_page->obj_list[i]) {
            if (ota_page->obj_list[i]->ui_free) {
                ota_page->obj_list[i]->ui_free(ota_page->obj_list[i]);
            }
            free(ota_page->obj_list[i]);
        }
    }
    free(ota_page->obj_list);
    ota_page->obj_list = NULL;
    ota_page->list_num = 0;
}

static int32_t ota_ui_upgrade_config_init(cJSON* root, ui_ota_t* ui_ota)
{
    cJSON* upgrading_page = NULL;
    cJSON* upgrade_fail_page = NULL;
    cJSON* upgrade_success_page = NULL;

    if (!root || !ui_ota)
        return -1;

    /* upgrading page parse */
    upgrading_page = cJSON_GetObjectItem(root, "upgrading_page");
    if (!cJSON_IsObject(upgrading_page)) {
        UI_LOG_ERROR("No upgrading_page found.\n");
        return -1;
    }

    if (ota_page_config_init(upgrading_page, &(ui_ota->upgrading_page)) < 0) {
        UI_LOG_ERROR("upgrading_page config parse error.\n");
        return -1;
    }

    /* upgrade_fail_page page parse */
    upgrade_fail_page = cJSON_GetObjectItem(root, "upgrade_fail_page");
    if (!cJSON_IsObject(upgrade_fail_page)) {
        UI_LOG_ERROR("No upgrade_fail_page found.\n");
        goto fail_page_err;
    }

    if (ota_page_config_init(upgrade_fail_page, &(ui_ota->upgrade_fail_page)) < 0) {
        UI_LOG_ERROR("upgrade_fail_page config parse error.\n");
        goto fail_page_err;
    }

    /* upgrade_success_page page parse */
    upgrade_success_page = cJSON_GetObjectItem(root, "upgrade_success_page");
    if (!cJSON_IsObject(upgrade_success_page)) {
        UI_LOG_ERROR("No upgrade_success_page found.\n");
        goto success_page_err;
    }

    if (ota_page_config_init(upgrade_success_page, &(ui_ota->upgrade_success_page)) < 0) {
        UI_LOG_ERROR("upgrade_success_page config parse error.\n");
        goto success_page_err;
    }

    return 0;
success_page_err:
    ota_page_config_destroy(&(ui_ota->upgrade_fail_page));
fail_page_err:
    ota_page_config_destroy(&(ui_ota->upgrading_page));
    return -1;
}

static int32_t ota_ui_logo_config_init(cJSON* root, ui_ota_t* ui_ota)
{
    cJSON* logo_page = NULL;

    if (!root || !ui_ota)
        return -1;

    /* logo page parse */
    logo_page = cJSON_GetObjectItem(root, "logo_page");
    if (!cJSON_IsObject(logo_page)) {
        UI_LOG_ERROR("No logo_page found.\n");
        return -1;
    }

    if (ota_page_config_init(logo_page, &(ui_ota->logo_page)) < 0) {
        UI_LOG_ERROR("logo_page config parse error.\n");
        return -1;
    }

    return 0;
}

int32_t ota_ui_config_init(const char* path, ui_ota_t* ui_ota, ui_mode_e mode)
{
    uint8_t* json_buf = NULL;
    int32_t ret = 0;
    cJSON *root = NULL, *bg_color = NULL;

    if (!path || !ui_ota) {
        return -1;
    }

    memset(ui_ota, 0, sizeof(ui_ota_t));

    json_buf = read_all_from_file(path);
    if (!json_buf) {
        UI_LOG_ERROR("ota load ui config error :%s\n", path);
        return -1;
    }

    root = cJSON_Parse((const char*)json_buf);
    if (!root) {
        free(json_buf);
        return -1;
    }

    /* background color parser, optional attr */
    bg_color = cJSON_GetObjectItem(root, "bg_color");
    if (cJSON_IsString(bg_color)) {
        ui_ota->bg_color = (uint32_t)strtoul(bg_color->valuestring, NULL, 16);
    } else {
        UI_LOG_INFO("use default background color!\n");
    }

    if (ota_ui_logo_config_init(root, ui_ota) < 0) {
        if (mode == OTA_UI_LOGO) {
            UI_LOG_ERROR("ota load logo pages config error!\n");
            ret = -1;
            goto error;
        }
    }

    if (ota_ui_upgrade_config_init(root, ui_ota) < 0) {
        if (mode == OTA_UI_UPGRADE) {
            UI_LOG_ERROR("ota load upgrade pages config error!\n");
            ret = -1;
            goto error;
        }
    }

error:
    if (ret < 0) {
        ota_ui_config_destroy(ui_ota);
    }
    cJSON_Delete(root);
    free(json_buf);
    return ret;
}

void ota_ui_config_destroy(ui_ota_t* ui_ota)
{
    ota_page_config_destroy(&(ui_ota->logo_page));
    ota_page_config_destroy(&(ui_ota->upgrading_page));
    ota_page_config_destroy(&(ui_ota->upgrade_fail_page));
    ota_page_config_destroy(&(ui_ota->upgrade_success_page));
}

void ota_ui_page_print(ui_ota_page_t* page)
{
    int32_t i = 0;
    ui_obj_t* tmp_ui_obj = NULL;
    ui_progress_t* tmp_progress_obj = NULL;
    if (!page || !page->obj_list)
        return;

    UI_LOG_DEBUG("------print page start-------\n");

    for (i = 0; i < page->list_num; i++) {
        tmp_ui_obj = page->obj_list[i];
        if (tmp_ui_obj) {
            UI_LOG_DEBUG("obj%d type:%d\n", i, tmp_ui_obj->type);
            UI_LOG_DEBUG("obj%d area:(%d,%d,%d,%d)\n", i, tmp_ui_obj->area.x0, tmp_ui_obj->area.y0, tmp_ui_obj->area.x1, tmp_ui_obj->area.y1);
            UI_LOG_DEBUG("obj%d align:(%d,%d)\n", i, tmp_ui_obj->align.align_hor, tmp_ui_obj->align.align_ver);
            if (tmp_ui_obj->type == UI_OBJ_PROGRESS) {
                tmp_progress_obj = (ui_progress_t*)tmp_ui_obj;
                UI_LOG_DEBUG("obj%d PROGRESS mode:%d\n", i, tmp_progress_obj->mode);
                UI_LOG_DEBUG("obj%d PROGRESS img list len:%d\n", i, tmp_progress_obj->img_map_len);
            }
        }
    }
    UI_LOG_DEBUG("------print page end-------\n");
}

void ota_ui_config_print(ui_ota_t* ui_ota)
{
    ota_ui_page_print(&(ui_ota->logo_page));
    ota_ui_page_print(&(ui_ota->upgrading_page));
    ota_ui_page_print(&(ui_ota->upgrade_fail_page));
    ota_ui_page_print(&(ui_ota->upgrade_success_page));
}
