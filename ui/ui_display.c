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

#include <fcntl.h>
#include <kvdb.h>
#include <nuttx/config.h>
#include <nuttx/video/fb.h>
#include <nuttx/video/rgbcolors.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "ui_common.h"

typedef struct {
    int fb_device;
    struct fb_planeinfo_s plane_info;
    struct fb_videoinfo_s video_info;
    void* fb_mem;
} fb_handle_t;

static int32_t check_dirty_area(ui_area_t* area)
{
    int32_t width = area->x1 - area->x0 + 1;
    int32_t height = area->y1 - area->y0 + 1;
    if (width <= 0 || height <= 0) {
        return -1;
    }
    return 0;
}

void ota_ui_set_area(ui_area_t* area, int32_t x, int32_t y, int32_t w, int32_t h)
{
    area->x0 = x;
    area->y0 = y;
    area->x1 = x + w - 1;
    area->y1 = y + h - 1;
}

static int32_t ota_ui_area_intersect(ui_area_t* area_res, ui_area_t* area0, ui_area_t* area1)
{
    area_res->x0 = UI_MAX(area0->x0, area1->x0);
    area_res->y0 = UI_MAX(area0->y0, area1->y0);

    area_res->x1 = UI_MIN(area0->x1, area1->x1);
    area_res->y1 = UI_MIN(area0->y1, area1->y1);

    if ((area_res->x0 > area_res->x1) || (area_res->y0 > area_res->y1)) {
        return -1;
    }
    return 0;
}

static void ota_ui_area_join(ui_area_t* area_res, ui_area_t* area0, ui_area_t* area1)
{
    area_res->x0 = UI_MIN(area0->x0, area1->x0);
    area_res->y0 = UI_MIN(area0->y0, area1->y0);

    area_res->x1 = UI_MAX(area0->x1, area1->x1);
    area_res->y1 = UI_MAX(area0->y1, area1->y1);
}

static void ota_ui_obj_invalid(ui_obj_t* obj)
{
    ui_ota_page_t* parent_page = obj->parent_page;
    ui_area_t* dirty_area = &(parent_page->dirty_area);
    ui_area_t* new_area = &(obj->area);
    if (check_dirty_area(dirty_area) < 0) {
        memcpy(dirty_area, new_area, sizeof(ui_area_t));
    } else {
        ota_ui_area_join(dirty_area, dirty_area, new_area);
    }
}

static void ota_ui_align_calc(ui_area_t* area, int w, int h, int horAlign, int verAlign)
{
    int32_t width = area->x1 - area->x0 + 1;
    int32_t height = area->y1 - area->y0 + 1;
    int32_t x_offset = 0;
    int32_t y_offset = 0;

    switch (horAlign) {
    case ALIGN_HOR_LEFT:
        break;
    case ALIGN_HOR_CENTER:
        x_offset = (width - w) / 2;
        break;
    case ALIGN_HOR_RIGHT:
        x_offset = width - w;
        break;
    default:
        break;
    };
    switch (verAlign) {
    case ALIGN_VER_TOP:
        break;
    case ALIGN_VER_CENTER:
        y_offset = (height - h) / 2;
        break;
    case ALIGN_VER_BOTTOM:
        y_offset = height - h;
        break;
    default:
        break;
    };

    area->x0 += x_offset;
    area->y0 += y_offset;
    area->x1 = area->x0 + w - 1;
    area->y1 = area->y0 + h - 1;
}

static uint32_t ota_ui_alpha_mix(uint32_t bg_color, uint32_t fg_color)
{
    uint8_t* p_bg_clr = (uint8_t*)&bg_color; /* background color */
    uint8_t* p_fg_clr = (uint8_t*)&fg_color; /* foreground color */

    uint8_t* p_bg_r = p_bg_clr + 0; /* background red   color */
    uint8_t* p_bg_g = p_bg_clr + 1; /* background green color */
    uint8_t* p_bg_b = p_bg_clr + 2; /* background blue  color */

    uint8_t* p_fg_r = p_fg_clr + 0; /* foreground red   color */
    uint8_t* p_fg_g = p_fg_clr + 1; /* foreground green color */
    uint8_t* p_fg_b = p_fg_clr + 2; /* foreground blue  color */
    uint8_t* p_fg_a = p_fg_clr + 3; /* alpha */

    *p_fg_r = ((*p_bg_r * (256 - *p_fg_a) + (*p_fg_r) * (*p_fg_a)) / 256);
    *p_fg_g = ((*p_bg_g * (256 - *p_fg_a) + (*p_fg_g) * (*p_fg_a)) / 256);
    *p_fg_b = ((*p_bg_b * (256 - *p_fg_a) + (*p_fg_b) * (*p_fg_a)) / 256);

    return fg_color;
}

static int32_t ota_ui_draw_fillrect(fb_handle_t* handle, ui_area_t* area, uint32_t color)
{
    int32_t i = 0, j = 0, offset = 0;
    uint32_t* fb_bpp32 = handle->fb_mem;
    ui_area_t draw_area, panel_area;
    const uint8_t bpp = handle->plane_info.bpp;
    const uint32_t xres = handle->video_info.xres;
    const uint32_t yres = handle->video_info.yres;

    ota_ui_set_area(&panel_area, 0, 0, xres, yres);

    if (bpp != 32) {
        UI_LOG_WARN("unsupported color depth :%d...\n", bpp);
        return -1;
    }

    if (ota_ui_area_intersect(&draw_area, area, &panel_area) < 0) {
        return -1;
    }

    for (j = draw_area.y0; j <= draw_area.y1; j++) {
        offset = j * xres;
        for (i = draw_area.x0; i <= draw_area.x1; i++) {
            *(fb_bpp32 + offset + i) = color;
        }
    }

    return 0;
}

static int32_t ota_ui_draw_clear_panel(fb_handle_t* handle, uint32_t color)
{
    const uint16_t xres = handle->video_info.xres;
    const uint16_t yres = handle->video_info.yres;
    ui_area_t panel_area;
    ota_ui_set_area(&panel_area, 0, 0, xres, yres);
    return ota_ui_draw_fillrect(handle, &panel_area, color);
}

static int32_t ota_ui_draw_img(fb_handle_t* handle, ui_area_t* area, ui_area_t* objArea, const uint8_t* imgBuf)
{
    int32_t i = 0, j = 0, offset = 0, img_offset = 0, img_offset_x = 0, img_offset_y = 0;
    img_head_t* img_head = (img_head_t*)imgBuf;
    ui_area_t draw_area;
    uint32_t* img_data = (uint32_t*)(imgBuf + sizeof(img_head_t));
    uint32_t* fb_bpp32 = handle->fb_mem;
    const uint8_t bpp = handle->plane_info.bpp;
    const uint16_t xres = handle->video_info.xres;

    if (bpp != 32) {
        UI_LOG_WARN("unsupported color depth :%d...\n", bpp);
        return -1;
    }

    if (ota_ui_area_intersect(&draw_area, area, objArea) < 0) {
        return -1;
    }

    img_offset_x = UI_MAX(0, draw_area.x0 - area->x0);
    img_offset_y = UI_MAX(0, draw_area.y0 - area->y0);

    for (i = 0; i <= draw_area.y1 - draw_area.y0; i++) {
        offset = (i + draw_area.y0) * xres + draw_area.x0;
        img_offset = (i + img_offset_y) * img_head->w + img_offset_x;
        for (j = 0; j <= draw_area.x1 - draw_area.x0; j++) {
            *(fb_bpp32 + offset + j) = ota_ui_alpha_mix(*(fb_bpp32 + offset + j), *(img_data + img_offset + j));
        }
    }
    return 0;
}

static void ui_label_draw(ui_obj_t* obj, void* fb_handle)
{
    ui_label_t* label = (ui_label_t*)obj;
    img_head_t* img_head = (img_head_t*)label->img_buffer;
    ui_ota_page_t* parent_page = obj->parent_page;
    ui_area_t tmp_area;
    ui_area_t clip_area;

    if (!img_head) {
        UI_LOG_WARN("NO image buffer found\n");
        return;
    }

    if (ota_ui_area_intersect(&clip_area, &(parent_page->dirty_area), &(obj->area)) == 0) {
        memcpy(&tmp_area, &obj->area, sizeof(ui_area_t));
        ota_ui_align_calc(&tmp_area, img_head->w, img_head->h, obj->align.align_hor, obj->align.align_ver);
        ota_ui_draw_img(fb_handle, &tmp_area, &clip_area, label->img_buffer);
    }
}

static void ui_label_free(ui_obj_t* obj)
{
    ui_label_t* label = (ui_label_t*)obj;
    if (label) {
        if (label->img_buffer) {
            free(label->img_buffer);
        }
        memset(label, 0, sizeof(ui_label_t));
    }
}

ui_label_t* ui_label_create(ui_ota_page_t* page)
{
    ui_label_t* new_label = malloc(sizeof(ui_label_t));
    ui_obj_t* obj = (ui_obj_t*)new_label;
    if (new_label) {
        memset(new_label, 0, sizeof(ui_label_t));
        obj->type = UI_OBJ_LABEL;
        obj->ui_draw = ui_label_draw;
        obj->ui_free = ui_label_free;
        obj->parent_page = page;
    }
    return new_label;
}

static void ui_progress_draw(ui_obj_t* obj, void* fb_handle)
{
    int32_t i = 0, tmp_val = 0, imgs_width = 0, imgs_height = 0;
    uint8_t* img_buffer = NULL;
    img_head_t* img_head = NULL;
    int32_t number_places[3] = { 0 };
    ui_progress_t* progress = (ui_progress_t*)obj;
    ui_ota_page_t* parent_page = progress->obj.parent_page;
    ui_area_t clip_area;
    ui_area_t tmp_area;
    ui_area_t number_area, precentage_area;

    if (ota_ui_area_intersect(&clip_area, &(parent_page->dirty_area), &(obj->area)) < 0) {
        return;
    }

    memcpy(&tmp_area, &obj->area, sizeof(ui_area_t));

    if (progress->mode == PROGRESS_MODE_NUMBER) {
        if (progress->img_map_len != 10) {
            UI_LOG_ERROR("progress img list length should be 10 in number mode,current:%ld\n", progress->img_map_len);
            return;
        }
        memset(number_places, -1, sizeof(number_places));
        tmp_val = progress->val;
        while (i < ARRAY_SIZE(number_places)) {
            number_places[i] = tmp_val % 10;
            tmp_val = tmp_val / 10;

            if (number_places[i] >= 0 && number_places[i] < progress->img_map_len) {
                img_head = (img_head_t*)progress->img_map[number_places[i]].img_buffer;
                if (img_head) {
                    imgs_width += img_head->w;
                    imgs_height = UI_MAX(imgs_height, img_head->h);
                }
            } else {
                UI_LOG_ERROR("unexpected index:%ld for progress:%ld\n", number_places[i], progress->val);
                return;
            }

            if (tmp_val == 0)
                break;
            i++;
        }

        if (progress->percentage_img_buffer) {
            img_head = (img_head_t*)progress->percentage_img_buffer;
            imgs_width += img_head->w;
            imgs_height = UI_MAX(imgs_height, img_head->h);
        }

        ota_ui_align_calc(&tmp_area, imgs_width, imgs_height, obj->align.align_hor, obj->align.align_ver);

        if (progress->percentage_img_buffer) {
            img_head = (img_head_t*)progress->percentage_img_buffer;
            imgs_width += img_head->w;
            ota_ui_set_area(&precentage_area, tmp_area.x1 - img_head->w + 1, tmp_area.y1 - img_head->h + 1, img_head->w, img_head->h);
            ota_ui_draw_img(fb_handle, &precentage_area, &clip_area, progress->percentage_img_buffer);
            tmp_area.x1 -= img_head->w - 1;
        }

        for (i = 0; i < ARRAY_SIZE(number_places); i++) {
            if (number_places[i] < 0) {
                break;
            }
            img_buffer = progress->img_map[number_places[i]].img_buffer;
            img_head = (img_head_t*)img_buffer;
            if (img_head) {
                ota_ui_set_area(&number_area, tmp_area.x1 - img_head->w + 1, tmp_area.y1 - img_head->h + 1, img_head->w, img_head->h);
                ota_ui_draw_img(fb_handle, &number_area, &clip_area, img_buffer);
                tmp_area.x1 -= img_head->w - 1;
            }
        }

    } else if (progress->mode == PROGRESS_MODE_BAR) {
        /* find out the img to display */
        for (i = 0; i < progress->img_map_len; i++) {
            if (progress->img_map[i].key == progress->val) {
                img_buffer = progress->img_map[i].img_buffer;
                break;
            } else if (i > 0 && progress->img_map[i].key > progress->val) {
                img_buffer = progress->img_map[i - 1].img_buffer;
                break;
            }
        }
        if (img_buffer) {
            img_head = (img_head_t*)img_buffer;
            ota_ui_align_calc(&tmp_area, img_head->w, img_head->h, obj->align.align_hor, obj->align.align_ver);
            ota_ui_draw_img(fb_handle, &tmp_area, &clip_area, img_buffer);
        } else {
            UI_LOG_WARN("can not find a correct bar to display.");
        }
    }
}

static void ui_progress_free(ui_obj_t* obj)
{
    ui_progress_t* progress = (ui_progress_t*)obj;
    int32_t i = 0;
    if (progress) {
        if (progress->percentage_img_buffer) {
            free(progress->percentage_img_buffer);
        }

        if (progress->img_map) {
            for (i = 0; i < progress->img_map_len; i++) {
                if (progress->img_map[i].img_buffer) {
                    free(progress->img_map[i].img_buffer);
                }
            }
            free(progress->img_map);
        }
        memset(progress, 0, sizeof(ui_progress_t));
    }
}

static void ui_progress_set_val(ui_progress_t* progress, int32_t val)
{
    if (val != progress->val) {
        progress->val = val;
        /* invalid obj */
        ota_ui_obj_invalid((ui_obj_t*)progress);
    }
}

ui_progress_t* ui_progress_create(ui_ota_page_t* page)
{
    ui_progress_t* new_progress = malloc(sizeof(ui_progress_t));
    ui_obj_t* obj = (ui_obj_t*)new_progress;
    if (new_progress) {
        memset(new_progress, 0, sizeof(ui_progress_t));
        obj->type = UI_OBJ_PROGRESS;
        obj->ui_draw = ui_progress_draw;
        obj->ui_free = ui_progress_free;
        obj->parent_page = page;
    }
    return new_progress;
}

static ui_progress_t* ui_page_find_progress(ui_ota_page_t* page)
{
    int32_t i = 0;
    if (page) {
        for (i = 0; i < page->list_num; i++) {
            if (page->obj_list[i]->type == UI_OBJ_PROGRESS) {
                return (ui_progress_t*)page->obj_list[i];
            }
        }
    }
    return NULL;
}

static void ota_ui_show_page(fb_handle_t* handle, ui_ota_page_t* page, uint32_t bg_color)
{
    int i = 0;
    ui_obj_t* obj = NULL;
    /* check dirty area */
    if (check_dirty_area(&(page->dirty_area)) < 0)
        return;

    /* draw bg */
    ota_ui_draw_fillrect(handle, &(page->dirty_area), bg_color);

    for (i = 0; i < page->list_num; i++) {
        obj = page->obj_list[i];
        if (obj && obj->ui_draw) {
            obj->ui_draw(obj, handle);
        }
    }
    /* clear dirty area */
    ota_ui_set_area(&(page->dirty_area), 0, 0, 0, 0);
}

static int32_t ota_fb_init(const char* dev_path, fb_handle_t* handle)
{
    if (!handle) {
        return -1;
    }
    handle->fb_device = open(dev_path, O_RDWR);

    if (handle->fb_device < 0) {
        UI_LOG_ERROR("fb device : %s open fail!\n", dev_path);
        goto fail;
    }

    if (ioctl(handle->fb_device, FBIOGET_PLANEINFO, &handle->plane_info)) {
        UI_LOG_ERROR("fb device get plane info error!\n");
        goto fail;
    }

    if (ioctl(handle->fb_device, FBIOGET_VIDEOINFO, &handle->video_info)) {
        UI_LOG_ERROR("fb device get video info error!\n");
        goto fail;
    }

    handle->fb_mem = mmap(NULL, handle->plane_info.fblen, PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_FILE, handle->fb_device, 0);
    if (handle->fb_mem == MAP_FAILED) {
        UI_LOG_ERROR("fb device mmap error!\n");
        goto fail;
    }
    return 0;
fail:
    if (handle->fb_device > 0) {
        close(handle->fb_device);
    }
    memset(handle, 0, sizeof(fb_handle_t));
    return -1;
}

static void ota_fb_destroy(fb_handle_t* handle)
{
    if (!handle) {
        return;
    }

    if (handle->fb_mem) {
        munmap(handle->fb_mem, handle->plane_info.fblen);
    }
    if (handle->fb_device > 0) {
        close(handle->fb_device);
    }
    memset(handle, 0, sizeof(fb_handle_t));
}

static int ota_upgrade_prop_get(const char* key)
{
    char buf[16] = { 0 };
    int ret = property_get(key, buf, NULL);
    if (ret < 0) {
        UI_LOG_ERROR("upgrade progress key(%s) not found\n", key);
        return 0;
    }

    ret = strtol(buf, NULL, 10);
    return ret;
}

static void ota_sync_upgrade_progress(upgrade_progress_t* progress)
{
    if (progress) {
        progress->cur = ota_upgrade_prop_get("ota.progress.current");
        progress->next = ota_upgrade_prop_get("ota.progress.next");
    }
}

static void ota_calc_display_progress(upgrade_progress_t* progress)
{
    static uint32_t slow_tick_count = 0, fast_tick_count = 0;
    if (progress) {
        if (progress->display < progress->cur) {
            if (slow_tick_count++ >= 30) {
                progress->display++;
                slow_tick_count = fast_tick_count = 0;
            }
        } else {
            if (fast_tick_count++ >= 300) {
                if (progress->display < progress->next) {
                    progress->display++;
                }
                slow_tick_count = fast_tick_count = 0;
            }
        }
    }
}

static void help_print(void)
{
    UI_LOG_INFO("\n Usage: otaUI [options]"
                "Options\n"
                "\t-t test mode.\t set the upgrade progress for test.\n"
                "\t\t 0 : progress current:0,progress next 20\n"
                "\t\t 1 : progress current:20,progress next 40\n"
                "\t\t 2 : progress current:40,progress next 60\n"
                "\t\t 3 : progress current:60,progress next 90\n"
                "\t\t 4 : progress current:90,progress next 100\n"
                "\t\t 5 : progress current:100,progress next 100\n"
                "\t\t 6 : progress current:-1,progress next 100\n"
                "\t-l logo mode.\t show logo,default is upgrade UI mode\n"
                "\t-c ota ui config path.\t default is " OTA_UI_DEFAULT_CONFIG_PATH "\n"
                "\t-c framebuffer dev path.\t default is " FB_DEFAULT_DEV_PATH "\n"
                "\t-h print help message.\n");
}

static void set_test_value(uint32_t val)
{
    static const char* current_list[] = { "0", "20", "40", "60", "90", "100", "-1" };
    static const char* next_list[] = { "20", "40", "60", "90", "100", "100", "100" };

    if (val >= ARRAY_SIZE(current_list)) {
        UI_LOG_WARN("test value :%lu is out of range\n", val);
        return;
    }

    property_set("ota.progress.current", current_list[val]);
    property_set("ota.progress.next", next_list[val]);
}

int main(int argc, char* argv[])
{
    ui_ota_t ui_ota;
    ui_mode_e ui_mode = OTA_UI_UPGRADE;
    fb_handle_t fb_handle;
    ui_progress_t* ui_progress = NULL;
    upgrade_progress_t upgrade_progress;
    uint32_t tickCount = 0;
    const uint32_t sync_progress_tick = UI_SYNC_OTA_PROGRESS_TICKS;
    int32_t option = 0;
    char config_path[128] = OTA_UI_DEFAULT_CONFIG_PATH;
    char dev_path[64] = FB_DEFAULT_DEV_PATH;

    while ((option = getopt(argc, argv, "t:c:d:lh")) != -1) {
        switch (option) {
        case 't':
            set_test_value(strtoul(optarg, NULL, 10));
            return 0;
        case 'c':
            snprintf(config_path, sizeof(config_path), "%s", optarg);
            break;
        case 'd':
            snprintf(dev_path, sizeof(dev_path), "%s", optarg);
            break;
        case 'l':
            ui_mode = OTA_UI_LOGO;
            break;
        case 'h':
            help_print();
            return 0;
        default:
            break;
        }
    }

    memset(&upgrade_progress, 0, sizeof(upgrade_progress_t));

    if (ota_ui_config_init(config_path, &ui_ota, ui_mode) < 0) {
        UI_LOG_ERROR("ota page config init error!\n");
        return 0;
    }

    ota_ui_config_print(&ui_ota);

    if (ota_fb_init(dev_path, &fb_handle) < 0) {
        UI_LOG_ERROR("ota framebuffer init error!\n");
        goto config_exit;
    }

    /* init dirty area for each page */
    ota_ui_set_area(&ui_ota.logo_page.dirty_area, 0, 0, fb_handle.video_info.xres, fb_handle.video_info.yres);
    ota_ui_set_area(&ui_ota.upgrading_page.dirty_area, 0, 0, fb_handle.video_info.xres, fb_handle.video_info.yres);
    ota_ui_set_area(&ui_ota.upgrade_fail_page.dirty_area, 0, 0, fb_handle.video_info.xres, fb_handle.video_info.yres);
    ota_ui_set_area(&ui_ota.upgrade_success_page.dirty_area, 0, 0, fb_handle.video_info.xres, fb_handle.video_info.yres);

    if (ui_mode == OTA_UI_LOGO) {
        ota_ui_show_page(&fb_handle, &ui_ota.logo_page, ui_ota.bg_color);
        goto fb_exit;
    }

    /* find out the progress widget first */
    ui_progress = ui_page_find_progress(&ui_ota.upgrading_page);

    while (1) {
        /* sync progress from ota system */
        if (tickCount % sync_progress_tick == 0) {
            ota_sync_upgrade_progress(&upgrade_progress);
        }
        if (upgrade_progress.cur < 0 || upgrade_progress.cur >= 100) {
            /* upgrade stopped */
            break;
        }

        ota_calc_display_progress(&upgrade_progress);

        /* set data to progress widget */
        if (ui_progress) {
            ui_progress_set_val(ui_progress, upgrade_progress.display);
        }

        ota_ui_show_page(&fb_handle, &ui_ota.upgrading_page, ui_ota.bg_color);

        tickCount++;
        usleep(UI_TICK_MS);
    }

    if (upgrade_progress.cur < 0) {
        ota_ui_show_page(&fb_handle, &ui_ota.upgrade_fail_page, ui_ota.bg_color);
    } else {
        ota_ui_show_page(&fb_handle, &ui_ota.upgrade_success_page, ui_ota.bg_color);
    }

    sleep(3);
    ota_ui_draw_clear_panel(&fb_handle, 0);
    usleep(32 * 1000);

fb_exit:
    ota_fb_destroy(&fb_handle);
config_exit:
    ota_ui_config_destroy(&ui_ota);
    return 0;
}
