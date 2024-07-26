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

/**
 * @file lv_upgrade.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_upgrade.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &lv_upgrade_class

#define LV_MAX(a, b) ((a) > (b) ? (a) : (b))
#define LV_MIN(a, b) ((a) < (b) ? (a) : (b))
#define LV_DEFAULT_ANIM_FPS (30)
#define LV_DEFAULT_CUSTOM_ANIM_WIDTH (80)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_upgrade_constructor(const lv_obj_class_t* class_p, lv_obj_t* obj);
static void lv_upgrade_destructor(const lv_obj_class_t* class_p, lv_obj_t* obj);
static void lv_upgrade_event(const lv_obj_class_t* class_p, lv_event_t* e);
static void lv_index_change(lv_obj_t* obj, int32_t index);
static void lv_image_data_free(void* data);
#if LV_USE_ARC != 0
static void lv_value_change(lv_obj_t* obj, int32_t index);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t lv_upgrade_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = lv_upgrade_constructor,
    .destructor_cb = lv_upgrade_destructor,
    .width_def = LV_PCT(100),
    .height_def = LV_PCT(100),
    .event_cb = lv_upgrade_event,
    .instance_size = sizeof(lv_upgrade_t),
};

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void lv_upgrade_constructor(const lv_obj_class_t* class_p, lv_obj_t* obj)
{
    LV_UNUSED(class_p);
    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
    upgrade->image_array_size = 0;
    upgrade->image_array = NULL;
    upgrade->image_percent_sign = NULL;
    lv_memset(upgrade->progress, -1, sizeof(upgrade->progress));
    upgrade->value = -1;

    /* init animation */
    upgrade->anim_fps = LV_DEFAULT_ANIM_FPS;
    lv_anim_init(&upgrade->anim);
    lv_anim_set_repeat_count(&upgrade->anim, LV_ANIM_REPEAT_INFINITE);

    lv_draw_image_dsc_init(&upgrade->image_dsc);
    lv_obj_init_draw_image_dsc(obj, LV_PART_MAIN, &upgrade->image_dsc);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
}

static void lv_upgrade_destructor(const lv_obj_class_t* class_p, lv_obj_t* obj)
{
    int i = 0;
    LV_ASSERT_OBJ(obj, MY_CLASS);
    LV_UNUSED(class_p);

    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
    if (upgrade->image_array) {
        while (i < upgrade->image_array_size) {
            if (upgrade->image_array[i]) {
                /* free file content */
                lv_image_data_free(upgrade->image_array[i]->data);
                /* free file info map */
                lv_free(upgrade->image_array[i]);
            }
            i++;
        }
        lv_free(upgrade->image_array);
        upgrade->image_array = NULL;
    }
    lv_image_data_free(upgrade->image_percent_sign);

    lv_anim_del(upgrade->anim.var, upgrade->anim.exec_cb);
}

static lv_area_t lv_calc_draw_area(void* image_src, lv_area_t* coords)
{
    uint32_t tmp_height = 0;
    lv_area_t res_area = { 0 };
    lv_image_header_t header;

    if (!image_src) {
        LV_LOG_ERROR("draw image data null !\n");
        return res_area;
    }

    tmp_height = coords->y2 - coords->y1;
    header = ((lv_image_dsc_t*)image_src)->header;

    /* interface "lv_draw_image" request coords end pos - 1 */
    coords->x1 = coords->x2 - header.w;
    coords->x2 = coords->x2 - 1;
    coords->y1 = coords->y1 - 1;
    coords->y2 = LV_MAX(tmp_height - 1, coords->y1 + header.h - 1);
    /* copy calc result area */
    lv_area_copy(&res_area, coords);
    /* reset start posX of the drawing */
    coords->x2 = coords->x1;

    return res_area;
}

static void lv_index_change(lv_obj_t* obj, int32_t index)
{
    image_data_t* image_data = NULL;
    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;

    int16_t anim_index = index % upgrade->image_array_size;
    if (anim_index < 0 || anim_index >= upgrade->image_array_size) {
        return;
    }
    image_data = upgrade->image_array[anim_index];
    if (!image_data) {
        return;
    }
    upgrade->anim_data = image_data->data;
    lv_obj_invalidate_area(obj, &obj->coords);
}

static void lv_image_data_free(void* data)
{
    lv_image_dsc_t* dsct = (lv_image_dsc_t*)data;

    if (dsct) {
        lv_free((void*)dsct->data);
        lv_free(dsct);
    }
}

#if LV_USE_ARC != 0
static void lv_value_change(lv_obj_t* obj, int32_t value)
{
    int32_t tmp_val = value;
    if (tmp_val >= 720) {
        tmp_val = 721;
    }

    lv_arc_set_value(obj, tmp_val % 360);
    lv_arc_set_start_angle(obj, value * 2 / 3);
}
#endif

static uint8_t lv_get_closest_element_index(lv_upgrade_t* upgrade, uint32_t value)
{
    int i;
    int target_index = 0;

    int min_diff = abs(value - upgrade->image_array[0]->key);
    for (i = 1; i < upgrade->image_array_size; i++) {
        int diff = abs(value - upgrade->image_array[i]->key);
        if (diff < min_diff) {
            min_diff = diff;
            target_index = i;
        }
    }
    /* progress bar display less than 100% when current progress is not 100% */
    if (min_diff != 0 && target_index == upgrade->image_array_size - 1) {
        return target_index - 1;
    }
    return target_index;
}

static void lv_render_number_mode_content(lv_obj_t* obj, lv_draw_task_t* draw_task)
{
    uint8_t i;
    lv_area_t coords;
    image_data_t* image_data = NULL;
    lv_area_t draw_area = { 0 };
    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
    lv_draw_dsc_base_t* base_dsc = draw_task->draw_dsc;

    lv_area_copy(&coords, &obj->coords);
    draw_area = lv_calc_draw_area(upgrade->image_percent_sign, &coords);
    lv_draw_image(base_dsc->layer, &upgrade->image_dsc, &draw_area);
    for (i = 0; i < LV_PROGRESS_DIGIT; i++) {
        if (upgrade->progress[i] < 0 || upgrade->progress[i] >= upgrade->image_array_size) {
            break;
        }
        image_data = upgrade->image_array[upgrade->progress[i]];
        if (!image_data) {
            continue;
        }
        draw_area = lv_calc_draw_area(image_data->data, &coords);
        lv_draw_image(base_dsc->layer, &upgrade->image_dsc, &draw_area);
    }
}

static void lv_render_bar_mode_content(lv_obj_t* obj, lv_draw_task_t* draw_task)
{
    lv_area_t coords;
    uint32_t cur_progress = 0;
    uint8_t target_index = 0;
    image_data_t* image_data = NULL;
    lv_area_t draw_area = { 0 };
    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
    lv_draw_dsc_base_t* base_dsc = draw_task->draw_dsc;

    lv_area_copy(&coords, &obj->coords);
    cur_progress = lv_upgrade_get_value((lv_obj_t*)upgrade);
    /* get closest key index in image array */
    target_index = lv_get_closest_element_index(upgrade, cur_progress);
    /* draw bar image */
    image_data = upgrade->image_array[target_index];
    if (!image_data) {
        return;
    }
    draw_area = lv_calc_draw_area(image_data->data, &coords);
    lv_area_align(&draw_task->area, &draw_area, LV_ALIGN_CENTER, 0, 0);

    lv_draw_rect_dsc_t draw_dsc;
    lv_draw_rect_dsc_init(&draw_dsc);
    draw_dsc.bg_color = lv_color_black();
    lv_draw_rect(base_dsc->layer, &draw_dsc, &draw_area);

    upgrade->image_dsc.src = image_data->data;
    lv_draw_image(base_dsc->layer, &upgrade->image_dsc, &draw_area);
}

static void lv_render_anim_mode_content(lv_obj_t* obj, lv_draw_task_t* draw_task)
{
    lv_area_t coords;
    lv_area_t draw_area = { 0 };
    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
    lv_draw_dsc_base_t* base_dsc = draw_task->draw_dsc;

    /* start animation */
    if (lv_anim_get(obj, (lv_anim_exec_xcb_t)lv_index_change) == NULL) {
        lv_anim_set_var(&upgrade->anim, obj);
        lv_anim_set_time(&upgrade->anim, upgrade->image_array_size * 1000 / upgrade->anim_fps);
        lv_anim_set_values(&upgrade->anim, 0, upgrade->image_array_size);
        lv_anim_set_exec_cb(&upgrade->anim, (lv_anim_exec_xcb_t)lv_index_change);
        lv_anim_start(&upgrade->anim);
        return;
    }

    lv_area_copy(&coords, &obj->coords);
    draw_area = lv_calc_draw_area(upgrade->anim_data, &coords);
    lv_draw_image(base_dsc->layer, &upgrade->image_dsc, &draw_area);
}

static void lv_render_custom_anim_content(lv_obj_t* obj, lv_draw_task_t* draw_task)
{
#if LV_USE_ARC != 0
    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
    /* start animation */
    if (lv_anim_get(upgrade->arc, (lv_anim_exec_xcb_t)lv_value_change) == NULL) {
        lv_anim_set_var(&upgrade->anim, upgrade->arc);
        lv_anim_set_time(&upgrade->anim, 1500);
        lv_anim_set_repeat_delay(&upgrade->anim, 500);
        lv_anim_set_values(&upgrade->anim, 0, 1080);
        lv_anim_set_path_cb(&upgrade->anim, lv_anim_path_ease_in_out);
        lv_anim_set_exec_cb(&upgrade->anim, (lv_anim_exec_xcb_t)lv_value_change);
        lv_anim_start(&upgrade->anim);
    }
#endif
}

static void lv_upgrade_event(const lv_obj_class_t* class_p, lv_event_t* e)
{
    LV_UNUSED(class_p);
    static int rotation = 0;

    lv_res_t res = lv_obj_event_base(&lv_upgrade_class, e);
    if (res != LV_RES_OK) {
        return;
    }
    /* Call the ancestor's event handler */
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DRAW_TASK_ADDED) {
        lv_draw_task_t* draw_task = lv_event_get_draw_task(e);
        lv_obj_t* obj = lv_event_get_target(e);
        if (!draw_task || !obj) {
            return;
        }
        lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
        switch (upgrade->type) {
        case LV_UPGRADE_TYPE_NUM:
            lv_render_number_mode_content(obj, draw_task);
            break;
        case LV_UPGRADE_TYPE_BAR:
            lv_render_bar_mode_content(obj, draw_task);
            break;
        case LV_UPGRADE_TYPE_CIRCLE:
            upgrade->image_dsc.rotation = rotation;
            upgrade->image_dsc.pivot.x = (obj->coords.x2 - obj->coords.x1) / 2;
            upgrade->image_dsc.pivot.y = (obj->coords.y2 - obj->coords.y1) / 2;
            lv_render_anim_mode_content(obj, draw_task);
            rotation += 360 / upgrade->anim_fps;
            break;
        case LV_UPGRADE_TYPE_ANIM:
            lv_render_anim_mode_content(obj, draw_task);
            break;
        case LV_UPGRADE_TYPE_CUSTOM_ANIM:
            lv_render_custom_anim_content(obj, draw_task);
            break;
        default:
            break;
        }
    }
}

static void lv_calc_objs_size(void* obj_data, lv_area_t* objs_area)
{
    lv_image_header_t* image_head = (lv_image_header_t*)(obj_data);
    if (image_head) {
        objs_area->x2 = objs_area->x2 + image_head->w;
        objs_area->y2 = LV_MAX(objs_area->y2 - objs_area->y1, image_head->h);
    }
}

static void lv_calc_number_mode_size(lv_upgrade_t* upgrade, lv_area_t* obj_area)
{
    uint8_t i;
    image_data_t* image_data = NULL;

    /* calc percent sign width */
    lv_calc_objs_size(upgrade->image_percent_sign, obj_area);
    for (i = 0; i < LV_PROGRESS_DIGIT; i++) {
        if (upgrade->progress[i] < 0 || upgrade->progress[i] >= upgrade->image_array_size) {
            break;
        }
        image_data = upgrade->image_array[upgrade->progress[i]];
        if (!image_data) {
            continue;
        }
        lv_calc_objs_size(image_data->data, obj_area);
    }
}

static void lv_calc_bar_mode_size(lv_upgrade_t* upgrade, lv_area_t* obj_area, uint32_t value)
{
    uint8_t target_index = 0;
    image_data_t* image_data = NULL;

    /* get closest key index in image array */
    target_index = lv_get_closest_element_index(upgrade, value);
    image_data = upgrade->image_array[target_index];
    if (!image_data) {
        return;
    }
    lv_calc_objs_size(image_data->data, obj_area);
}

static void lv_calc_anim_mode_size(lv_upgrade_t* upgrade, lv_area_t* obj_area)
{
    uint8_t i;
    image_data_t* image_data = NULL;

    /* calculate max size from image array */
    for (i = 0; i < upgrade->image_array_size; i++) {
        image_data = upgrade->image_array[i];
        if (!image_data) {
            continue;
        }
        lv_image_header_t* image_head = (lv_image_header_t*)(image_data->data);
        if (image_head) {
            obj_area->x2 = LV_MAX(obj_area->x2 - obj_area->x1, image_head->w);
            obj_area->y2 = LV_MAX(obj_area->y2 - obj_area->y1, image_head->h);
        }
    }
}

static void lv_calc_custom_anim_size(lv_upgrade_t* upgrade, lv_area_t* obj_area)
{
#if LV_USE_ARC != 0
    obj_area->x2 = upgrade->arc_radius;
    obj_area->y2 = upgrade->arc_radius;
    lv_obj_set_size(upgrade->arc, obj_area->x2 - obj_area->x1, obj_area->y2 - obj_area->y1);
#endif
}

static void read_data_from_file(const char* path, lv_image_dsc_t* dsct)
{
    int fd = 0;

    fd = open(path, O_RDONLY);

    if (fd < 0) {
        LV_LOG_ERROR("open file failed!");
        return;
    }

    off_t flen = lseek(fd, 0L, SEEK_END);
    if (flen < sizeof(lv_image_header_t)) {
        LV_LOG_ERROR("read file failed!");
        goto r_end;
    }

    off_t sset = lseek(fd, 0L, SEEK_SET);
    if (sset < 0) {
        LV_LOG_ERROR("seek file error!");
        goto r_end;
    }

    int ret = read(fd, &dsct->header, sizeof(lv_image_header_t));
    if (ret < 0) {
        LV_LOG_ERROR("read file failed!");
        goto r_end;
    }

    flen -= sizeof(lv_image_header_t);
    dsct->data = lv_malloc(flen);

    sset = lseek(fd, sizeof(lv_image_header_t), SEEK_SET);
    if (sset < 0) {
        LV_LOG_ERROR("seek file error!");
        goto r_end;
    }

    ret = read(fd, (uint8_t*)dsct->data, flen);
    if (ret < 0) {
        LV_LOG_ERROR("read file failed!");
    }

r_end:
    close(fd);
}

static void* get_image_data_from_file(const char* path)
{
    lv_image_dsc_t* dsct = lv_malloc(sizeof(lv_image_dsc_t));
    lv_memzero(dsct, sizeof(lv_image_dsc_t));

    read_data_from_file(path, dsct);

    return dsct;
}

lv_obj_t* lv_upgrade_create(lv_obj_t* parent)
{
    lv_obj_t* obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void lv_upgrade_set_type(lv_obj_t* obj, lv_upgrade_type_e type)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
    upgrade->type = type;

#if LV_USE_ARC != 0
    if (type == LV_UPGRADE_TYPE_CUSTOM_ANIM) {
        /* creaet arc obj */
        upgrade->arc = lv_arc_create(obj);
        upgrade->arc_radius = LV_DEFAULT_CUSTOM_ANIM_WIDTH;
        lv_arc_set_rotation(upgrade->arc, 270);
        lv_arc_set_bg_angles(upgrade->arc, 0, 360);
        lv_arc_set_range(upgrade->arc, 0, 360);
        lv_obj_remove_style(upgrade->arc, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(upgrade->arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(upgrade->arc, 8, 0);
        lv_obj_set_style_arc_width(upgrade->arc, 8, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(upgrade->arc, lv_color_make(69, 73, 79), 0);
        lv_obj_set_style_arc_color(upgrade->arc, lv_color_make(167, 170, 178), LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(upgrade->arc, 5, LV_PART_INDICATOR);
        lv_obj_center(upgrade->arc);
    }
#endif
}

void lv_upgrade_set_animation_fps(lv_obj_t* obj, int fps)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
    upgrade->anim_fps = fps;
}

void lv_upgrade_set_animation_radius(lv_obj_t* obj, int radius)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

#if LV_USE_ARC != 0
    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
    upgrade->arc_radius = radius;
#endif
}

void lv_upgrade_set_percent_sign_image(lv_obj_t* obj, const char* file_path)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;

    upgrade->image_percent_sign = get_image_data_from_file(file_path);
}

void lv_upgrade_set_image_array_size(lv_obj_t* obj, int size)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;

    if (upgrade->image_array_size > 0 || size <= 0) {
        LV_LOG_ERROR("image array should use only once and new size at least 1!\n");
        return;
    }

    upgrade->image_array_size = size;
    upgrade->image_array = (image_data_t**)lv_malloc(size * sizeof(image_data_t*));

    lv_memzero(upgrade->image_array, size * sizeof(void*));
}

void lv_upgrade_set_image_data(lv_obj_t* obj, int key, const char* file_path)
{
    image_data_t* image_data = NULL;
    int i = 0;

    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;

    /* in number mode, list should be order like [0.1.2.3.4.5.6.7.8.9] */
    if (upgrade->type == LV_UPGRADE_TYPE_NUM) {
        i = key;
    } else {
        /* find a null node in array list */
        while (i < upgrade->image_array_size && upgrade->image_array[i]) {
            i++;
        }
    }

    if (i >= upgrade->image_array_size) {
        LV_LOG_ERROR("image array full !\n");
        return;
    }

    image_data = (image_data_t*)lv_malloc(sizeof(image_data_t));
    image_data->key = key;
    image_data->data = get_image_data_from_file(file_path);
    upgrade->image_array[i] = image_data;
}

void lv_upgrade_set_progress(lv_obj_t* obj, uint32_t value)
{
    uint8_t i = 0;
    lv_area_t new_obj_area = { 0 };
    int tmp_value = 0;

    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
    if (upgrade->value >= 0 && upgrade->value == value) {
        return;
    }

    lv_memset(upgrade->progress, -1, sizeof(upgrade->progress));

    tmp_value = value;
    while (i < LV_PROGRESS_DIGIT) {
        upgrade->progress[i] = tmp_value % 10;
        tmp_value = tmp_value / 10;
        if (tmp_value == 0) {
            break;
        }
        i++;
    }
    upgrade->value = value;

    switch (upgrade->type) {
    case LV_UPGRADE_TYPE_NUM:
        lv_calc_number_mode_size(upgrade, &new_obj_area);
        break;
    case LV_UPGRADE_TYPE_BAR:
        lv_calc_bar_mode_size(upgrade, &new_obj_area, value);
        break;
    case LV_UPGRADE_TYPE_CIRCLE:
        lv_calc_anim_mode_size(upgrade, &new_obj_area);
        break;
    case LV_UPGRADE_TYPE_ANIM:
        lv_calc_anim_mode_size(upgrade, &new_obj_area);
        break;
    case LV_UPGRADE_TYPE_CUSTOM_ANIM:
        lv_calc_custom_anim_size(upgrade, &new_obj_area);
        break;
    default:
        break;
    }

    lv_obj_set_size(obj, new_obj_area.x2 - new_obj_area.x1, new_obj_area.y2 - new_obj_area.y1);

    lv_obj_invalidate(obj);
    lv_refr_now(NULL);
}

uint32_t lv_upgrade_get_value(lv_obj_t* obj)
{
    uint8_t i = 0;
    uint32_t result = 0;
    uint32_t inc_value = 1;
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
    while (i < LV_PROGRESS_DIGIT) {
        if (upgrade->progress[i] < 0) {
            break;
        }
        result += upgrade->progress[i] * inc_value;
        inc_value *= 10;
        i++;
    }

    return result;
}
