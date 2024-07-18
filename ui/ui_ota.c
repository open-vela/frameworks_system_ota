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

#include <fcntl.h>
#include <kvdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_common.h"

#define CONFIG_PATH_SIZE (128)
#define UI_DEFAULT_CONFIG_PATH "/resource/recovery/ota_ui_config.json"
#define SYNC_OTA_PROGRESS_TIME CONFIG_OTA_UI_PROG_SYNC_TIME

static const char* logo_page_name = "logo_page";
static const char* upgrading_page_name = "upgrading_page";
static const char* upgrade_success_page_name = "upgrade_success_page";
static const char* upgrade_fail_page_name = "upgrade_fail_page";
static const char* reboot_cmd = CONFIG_OTA_UI_FINISH_EXEC_CMD;
static const int cmd_wait_time = CONFIG_OTA_UI_CMD_WAIT_TIME;
typedef struct upgrade_progress_s {
    int32_t current;
    int32_t prev_node;
    int32_t next_node;
} upgrade_progress_t;

static void ota_calc_progress(upgrade_progress_t* progress)
{
    static uint32_t slow_tick_count = 0, fast_tick_count = 0;
    if (progress) {
        if (progress->current < progress->prev_node && fast_tick_count++ >= 2) {
            progress->current++;
            slow_tick_count = fast_tick_count = 0;
        } else {
            if (progress->current < progress->next_node && slow_tick_count++ >= 100) {
                progress->current++;
                slow_tick_count = fast_tick_count = 0;
            }
        }
        if (progress->current >= 100 && progress->prev_node < 100) {
            progress->current = 99;
        }
    }
}

static int ota_upgrade_prop_get(const char* key)
{
    char buf[16] = { 0 };
    if (property_get(key, buf, NULL) < 0) {
        UI_LOG_ERROR("ota: upgrade progress key(%s) not found\n", key);
        return 0;
    }

    return strtol(buf, NULL, 10);
}

static void ota_sync_upgrade_progress(upgrade_progress_t* progress)
{
    if (progress) {
        progress->prev_node = ota_upgrade_prop_get("ota.progress.current");
        progress->next_node = ota_upgrade_prop_get("ota.progress.next");
    }
}

static void set_test_value(uint32_t val)
{
    static const char* prev_list[] = { "0", "20", "40", "60", "90", "100", "-1" };
    static const char* next_list[] = { "20", "40", "60", "90", "100", "100", "100" };

    if (val >= ARRAY_SIZE(prev_list)) {
        UI_LOG_WARN("ota: test value :%u is out of range\n", (unsigned int)val);
        return;
    }

    property_set("ota.progress.current", prev_list[val]);
    property_set("ota.progress.next", next_list[val]);
}

static void help_print(void)
{
    printf("\n Usage: otaUI [options]"
           "Options\n"
           "\t-t test mode.\t set the upgrade progress for test.\n"
           "\t\t 0 : progress prev node:0, progress next node:20\n"
           "\t\t 1 : progress prev node:20, progress next node:40\n"
           "\t\t 2 : progress prev node:40, progress next node:60\n"
           "\t\t 3 : progress prev node:60, progress next node:90\n"
           "\t\t 4 : progress prev node:90, progress next node:100\n"
           "\t\t 5 : progress prev node:100, progress next node:100\n"
           "\t\t 6 : progress prev node:-1, progress next node:100\n"
           "\t-l logo mode.\t show logo,default is upgrade UI mode\n"
           "\t-p set upgrade progress start value.\n"
           "\t-c ui config path.\t default is " UI_DEFAULT_CONFIG_PATH "\n"
           "\t-h print help message.\n");
}

int main(int argc, char* argv[])
{
    int32_t option = 0;
    char config_path[CONFIG_PATH_SIZE] = { 0 };
    upgrade_progress_t progress = { 0 };
    bool logo_mode = false;
    ui_result_code_e ret = UI_SUCCESS;
    uint32_t sync_progress_time = SYNC_OTA_PROGRESS_TIME;
    int32_t count_tick_ms = 0;

    while ((option = getopt(argc, argv, "t:c:p:lh")) != -1) {
        switch (option) {
        case 't':
            set_test_value(strtoul(optarg, NULL, 10));
            return 0;
        case 'c':
            snprintf(config_path, sizeof(config_path) - 1, "%s", optarg);
            break;
        case 'p':
            progress.current = strtoul(optarg, NULL, 10);
            break;
        case 'l':
            logo_mode = true;
            break;
        case 'h':
            help_print();
            return 0;
        default:
            break;
        }
    }

    ui_init();

    if (strlen(config_path) <= 0) {
        snprintf(config_path, sizeof(config_path), "%s", UI_DEFAULT_CONFIG_PATH);
    }

    if (ui_config_parse(config_path) < 0) {
        return -1;
    }

    while (1) {
        if (count_tick_ms % sync_progress_time == 0) {
            ota_sync_upgrade_progress(&progress);
        }
        if (logo_mode) {
            ret = ui_page_show(logo_page_name);
        } else {
            if (progress.prev_node < 0) {
                ret = ui_page_show(upgrade_fail_page_name);
            } else {
                ret = ui_page_show(upgrading_page_name);
                ota_calc_progress(&progress);
                ret = ui_set_progress(upgrading_page_name, progress.current);

                if (ui_page_exist(upgrade_success_page_name) && progress.current >= 100) {
                    ret = ui_page_show(upgrade_success_page_name);
                    ui_timer_handler();
                }
            }
        }

        if (ret != UI_SUCCESS) {
            break;
        }

        count_tick_ms += UI_TICK_MS;

        ui_timer_handler();
        if (logo_mode || progress.prev_node < 0 || progress.current >= 100) {
            sleep(cmd_wait_time);
            break;
        }
    }

    ui_uninit();

    ota_sync_upgrade_progress(&progress);
    if (progress.prev_node < 0 || progress.prev_node > 100) {
        system(reboot_cmd);
    }

    return 0;
}