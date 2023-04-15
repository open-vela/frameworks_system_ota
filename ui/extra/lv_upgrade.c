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

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_upgrade_constructor(const lv_obj_class_t* class_p, lv_obj_t* obj);
static void lv_upgrade_destructor(const lv_obj_class_t* class_p, lv_obj_t* obj);
static void lv_upgrade_event(const lv_obj_class_t* class_p, lv_event_t* e);

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
    upgrade->value = -1;
    lv_memset(upgrade->progress, -1, sizeof(upgrade->progress));
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
                if (upgrade->image_array[i]->data) {
                    free(upgrade->image_array[i]->data);
                }
                /* free file info map */
                free(upgrade->image_array[i]);
            }
            i++;
        }
        free(upgrade->image_array);
        upgrade->image_array = NULL;
    }
    if (upgrade->image_percent_sign) {
        free(upgrade->image_percent_sign);
        upgrade->image_percent_sign = NULL;
    }
}

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
    return target_index;
}

static void lv_draw_child_image(lv_draw_ctx_t* draw_ctx, lv_upgrade_t* lv_obj, uint32_t* img_src, lv_area_t* coords)
{
    uint32_t tmp_height = 0;
    lv_draw_img_dsc_t img_dsc;
    lv_img_header_t header;

    if (!img_src) {
        LV_LOG_ERROR("draw image data null !\n");
        return;
    }

    lv_draw_img_dsc_init(&img_dsc);
    lv_obj_init_draw_img_dsc((lv_obj_t*)lv_obj, LV_PART_MAIN, &img_dsc);

    tmp_height = coords->y2 - coords->y1;
    lv_img_decoder_get_info(img_src, &header);

    coords->x1 = coords->x2 - header.w;
    /* interface function "lv_draw_img" request coords end pos - 1 */
    coords->x2 = coords->x2 - 1;
    coords->y2 = LV_MAX(tmp_height - 1, coords->y1 + header.h - 1);
    lv_draw_img(draw_ctx, &img_dsc, coords, img_src);

    /* reset start posX of the drawing */
    coords->x2 = coords->x1;
}

static void lv_upgrade_event(const lv_obj_class_t* class_p, lv_event_t* e)
{
    LV_UNUSED(class_p);

    uint8_t i;
    uint32_t cur_progress = 0;
    uint8_t target_index = 0;
    image_data_t* image_data = NULL;

    lv_res_t res = lv_obj_event_base(&lv_upgrade_class, e);
    if (res != LV_RES_OK)
        return;

    /* Call the ancestor's event handler */
    lv_event_code_t code = lv_event_get_code(e);
    lv_draw_ctx_t* draw_ctx = lv_event_get_draw_ctx(e);
    lv_obj_t* obj = lv_event_get_target(e);

    if (code == LV_EVENT_DRAW_MAIN) {
        lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
        if (!upgrade) {
            return;
        }

        lv_area_t coords;
        lv_area_copy(&coords, &obj->coords);

        if (upgrade->type == LV_UPGRADE_TYPE_NUM) {
            lv_draw_child_image(draw_ctx, upgrade, upgrade->image_percent_sign, &coords);
            for (i = 0; i < LV_PROGRESS_DIGIT; i++) {
                if (upgrade->progress[i] < 0 || upgrade->progress[i] >= upgrade->image_array_size) {
                    break;
                }
                image_data = upgrade->image_array[upgrade->progress[i]];
                if (!image_data) {
                    continue;
                }
                lv_draw_child_image(draw_ctx, upgrade, image_data->data, &coords);
            }
        } else if (upgrade->type == LV_UPGRADE_TYPE_BAR) {
            cur_progress = lv_upgrade_get_value(obj);
            /* get closest key index in image array */
            target_index = lv_get_closest_element_index(upgrade, cur_progress);
            /* draw bar image */
            image_data = upgrade->image_array[target_index];
            if (!image_data) {
                return;
            }
            lv_draw_child_image(draw_ctx, upgrade, image_data->data, &coords);
        }
    }
}

static void lv_calc_obj_size(lv_upgrade_t* lv_obj, void* data, lv_coord_t* width, lv_coord_t* height)
{
    lv_img_header_t* img_head = (lv_img_header_t*)(data);
    if (img_head) {
        *width += img_head->w;
        *height = (lv_coord_t)LV_MAX(img_head->h, *height);
    }
}

static uint8_t* read_all_from_file(const char* path, uint32_t* data_size)
{
    int fd = 0;
    size_t flen = 0;
    uint8_t* content = NULL;

    fd = open(path, O_RDONLY);

    if (fd < 0) {
        LV_LOG_ERROR("open file failed!");
        return NULL;
    }

    flen = lseek(fd, 0L, SEEK_END);
    lseek(fd, 0L, SEEK_SET);

    content = malloc(flen + 1);
    if (data_size) {
        *data_size = flen + 1;
    }

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
    uint8_t* img_buff = read_all_from_file(path, &data_size);
    if (!img_buff) {
        LV_LOG_ERROR("read file error !\n");
        return NULL;
    }

    lv_img_dsc_t* dsc = (lv_img_dsc_t*)img_buff;
    dsc->data_size = data_size - sizeof(lv_img_header_t);
    dsc->data = img_buff + sizeof(lv_img_header_t);

    return img_buff;
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
    upgrade->image_array = (image_data_t**)malloc(size * sizeof(image_data_t*));

    memset(upgrade->image_array, 0, size * sizeof(void*));
}

void lv_upgrade_set_image_data(lv_obj_t* obj, int key, const char* file_path)
{
    image_data_t* image_data = NULL;
    int i = 0;

    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;

    if (upgrade->type == LV_UPGRADE_TYPE_NUM) {
        i = key;
    } else if (upgrade->type == LV_UPGRADE_TYPE_BAR) {
        /* find null array node */
        while (i < upgrade->image_array_size && upgrade->image_array[i]) {
            i++;
        }
    }

    if (i >= upgrade->image_array_size) {
        LV_LOG_ERROR("image array full !\n");
        return;
    }

    image_data = (image_data_t*)malloc(sizeof(image_data_t));
    image_data->key = key;
    image_data->data = get_image_data_from_file(file_path);
    upgrade->image_array[i] = image_data;
}

void lv_upgrade_set_progress(lv_obj_t* obj, uint32_t value)
{
    uint8_t i = 0;
    int tmp_value = 0;
    uint8_t target_index = 0;
    lv_coord_t width = 0;
    lv_coord_t height = 0;
    image_data_t* image_data = NULL;

    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_upgrade_t* upgrade = (lv_upgrade_t*)obj;
    lv_memset(upgrade->progress, -1, sizeof(upgrade->progress));

    if (upgrade->value >= 0 && upgrade->value == value) {
        return;
    }

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
    if (upgrade->type == LV_UPGRADE_TYPE_NUM) {
        /* calc percent sign width */
        lv_calc_obj_size(upgrade, upgrade->image_percent_sign, &width, &height);
        for (i = 0; i < LV_PROGRESS_DIGIT; i++) {
            if (upgrade->progress[i] < 0 || upgrade->progress[i] >= upgrade->image_array_size) {
                break;
            }
            image_data = upgrade->image_array[upgrade->progress[i]];
            if (!image_data) {
                continue;
            }
            lv_calc_obj_size(upgrade, image_data->data, &width, &height);
        }
    } else if (upgrade->type == LV_UPGRADE_TYPE_BAR) {
        /* get closest key index in image array */
        target_index = lv_get_closest_element_index(upgrade, value);
        image_data = upgrade->image_array[target_index];
        if (!image_data) {
            return;
        }
        /* calc bar width */
        lv_calc_obj_size(upgrade, image_data->data, &width, &height);
    }

    lv_obj_set_size(obj, width, height);

    lv_obj_invalidate(obj);
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
