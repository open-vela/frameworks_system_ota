/**
 * @file lv_upgrade.h
 *
 */

#ifndef _LV_UPGRADE_H_
#define _LV_UPGRADE_H_

#include <syslog.h>

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <lvgl/lvgl.h>
#include <stdlib.h>

/**********************
 *      TYPEDEFS
 **********************/
#define LV_PROGRESS_DIGIT (3)

typedef enum {
    LV_UPGRADE_TYPE_NUM,
    LV_UPGRADE_TYPE_BAR,
    LV_UPGRADE_TYPE_INVALID
} lv_upgrade_type_e;

typedef struct {
    int key;
    void* data;
} image_data_t;

/*Data of upgrade*/
typedef struct {
    lv_obj_t obj;
    lv_upgrade_type_e type;
    uint8_t image_array_size;
    image_data_t** image_array;
    void* image_percent_sign;
    int8_t progress[LV_PROGRESS_DIGIT]; /* Current value of the upgrade, buffer with inverted byte array, like: 100->{0,0,1}, 50->{0,5,-1} */
    volatile int8_t value; /* sync with above param progress[] */
} lv_upgrade_t;

extern const lv_obj_class_t lv_upgrade_class;

/**
 * Create a upgrade object
 * @param parent pointer to an object, it will be the parent of the new upgrade
 * @return pointer to the created upgrade
 */
lv_obj_t* lv_upgrade_create(lv_obj_t* parent);

/**
 * Set upgrade type
 * @param type upgrade show type
*/
void lv_upgrade_set_type(lv_obj_t* obj, lv_upgrade_type_e type);

/**
 * Set percent sign image file path
 * @param obj pointer to a upgrade object
 * @param file_path pointer to file path
*/
void lv_upgrade_set_percent_sign_image(lv_obj_t* obj, const char* file_path);

/**
 * Set upgrade image buff size
 * @param obj pointer to a upgrade object
 * @param size image buffer array size
*/
void lv_upgrade_set_image_array_size(lv_obj_t* obj, int size);

/**
 * Set progress image file path
 * @param obj pointer to a upgrade object
 * @param key image file key, in num mode rely to lv_upgrade_set_image_array_size value
 * @param file_path image file path
*/
void lv_upgrade_set_image_data(lv_obj_t* obj, int key, const char* file_path);

/**
 * Set the value of the upgrade
 * @param obj       pointer to a upgrade object
 * @param value     the value of the upgrade
 */
void lv_upgrade_set_progress(lv_obj_t* obj, uint32_t value);

/**
 * Get the value of the upgrade
 * @param obj pointer to a upgrade object
 * @return return value of current upgrade
*/
uint32_t lv_upgrade_get_value(lv_obj_t* obj);

#ifdef __cplusplus
}
#endif

#endif
