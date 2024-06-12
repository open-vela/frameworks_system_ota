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
#include <stdint.h>
#include <stdio.h>
#include <sys/boardctl.h>
#include <syslog.h>

#include <kvdb.h>

#include "bootctl.h"

#define BOOTCTL_SLOT_A_PATH CONFIG_UTILS_BOOTCTL_SLOT_A
#define BOOTCTL_SLOT_A_ACTIVE "persist.boot.slot_a.active"
#define BOOTCTL_SLOT_A_BOOTABLE "persist.boot.slot_a.bootable"
#define BOOTCTL_SLOT_A_SUCCESSFUL "persist.boot.slot_a.successful"

#define BOOTCTL_SLOT_B_PATH CONFIG_UTILS_BOOTCTL_SLOT_B
#define BOOTCTL_SLOT_B_ACTIVE "persist.boot.slot_b.active"
#define BOOTCTL_SLOT_B_BOOTABLE "persist.boot.slot_b.bootable"
#define BOOTCTL_SLOT_B_SUCCESSFUL "persist.boot.slot_b.successful"
#define BOOTCTL_SLOT_TRY "persist.boot.try"

#ifdef CONFIG_UTILS_BOOTCTL_DEBUG
#define BOOTCTL_LOG(l, f, ...) syslog(l, "%s:%d: " f, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define BOOTCTL_LOG(l, f, ...)
#endif

struct bootctl_slot_s {
    bool active; /* 0: inactive, 1: active */
    bool bootable; /* mark the slot maybe can boot */
    bool successful; /* mark the slot can boot */
};

struct bootctl_s {
    struct bootctl_slot_s slot[2];
    bool try;
};

const char g_bootctl_slot_a[] = CONFIG_UTILS_BOOTCTL_SLOT_A;
const char g_bootctl_slot_b[] = CONFIG_UTILS_BOOTCTL_SLOT_B;

static int bootctl_write_config(struct bootctl_s* boot)
{
    int ret;

    ret = property_set_bool(BOOTCTL_SLOT_A_ACTIVE, boot->slot[0].active);
    if (ret < 0) {
        BOOTCTL_LOG(LOG_ERR, "set slot a active failed, ret: %d", ret);
        return ret;
    }

    ret = property_set_bool(BOOTCTL_SLOT_A_BOOTABLE, boot->slot[0].bootable);
    if (ret < 0) {
        BOOTCTL_LOG(LOG_ERR, "set slot a bootable failed, ret: %d", ret);
        return ret;
    }

    ret = property_set_bool(BOOTCTL_SLOT_A_SUCCESSFUL, boot->slot[0].successful);
    if (ret < 0) {
        BOOTCTL_LOG(LOG_ERR, "set slot a successful failed, ret: %d", ret);
        return ret;
    }

    ret = property_set_bool(BOOTCTL_SLOT_B_ACTIVE, boot->slot[1].active);
    if (ret < 0) {
        BOOTCTL_LOG(LOG_ERR, "set slot b active failed, ret: %d", ret);
        return ret;
    }

    ret = property_set_bool(BOOTCTL_SLOT_B_BOOTABLE, boot->slot[1].bootable);
    if (ret < 0) {
        BOOTCTL_LOG(LOG_ERR, "set slot b bootable failed, ret: %d", ret);
        return ret;
    }

    ret = property_set_bool(BOOTCTL_SLOT_B_SUCCESSFUL, boot->slot[1].successful);
    if (ret < 0) {
        BOOTCTL_LOG(LOG_ERR, "set slot b successful failed, ret: %d", ret);
        return ret;
    }

    ret = property_set_bool(BOOTCTL_SLOT_TRY, boot->try);
    if (ret < 0) {
        BOOTCTL_LOG(LOG_ERR, "set try failed, ret: %d", ret);
        return ret;
    }

    return property_commit();
}

static void bootctl_read_config(struct bootctl_s* boot)
{
    boot->slot[0].active = property_get_bool(BOOTCTL_SLOT_A_ACTIVE, false);
    boot->slot[0].bootable = property_get_bool(BOOTCTL_SLOT_A_BOOTABLE, false);
    boot->slot[0].successful = property_get_bool(BOOTCTL_SLOT_A_SUCCESSFUL, false);
    boot->slot[1].active = property_get_bool(BOOTCTL_SLOT_B_ACTIVE, false);
    boot->slot[1].bootable = property_get_bool(BOOTCTL_SLOT_B_BOOTABLE, false);
    boot->slot[1].successful = property_get_bool(BOOTCTL_SLOT_B_SUCCESSFUL, false);
    boot->try = property_get_bool(BOOTCTL_SLOT_TRY, false);
}

#ifndef CONFIG_UTILS_BOOTCTL_ENTRY

/* get the active slot */
const char* bootctl_active(void)
{
    struct bootctl_s boot;

    bootctl_read_config(&boot);
    if (boot.slot[0].active) {
        return g_bootctl_slot_a;
    } else if (boot.slot[1].active) {
        return g_bootctl_slot_b;
    }
    return NULL;
}

/* boot update, clear the slot successful and bootable, run when update slot */
int bootctl_update(void)
{
    struct bootctl_s boot;

    bootctl_read_config(&boot);
    if (boot.slot[0].active) {
        boot.slot[1].successful = false;
        boot.slot[1].bootable = false;
        boot.slot[1].active = false;
        BOOTCTL_LOG(LOG_INFO, "update slot a");
    } else {
        boot.slot[0].successful = false;
        boot.slot[0].bootable = false;
        boot.slot[0].active = false;
        BOOTCTL_LOG(LOG_INFO, "update slot b");
    }

    return bootctl_write_config(&boot);
}

/* update slot done, switch active slot */

int bootctl_done(void)
{
    struct bootctl_s boot;

    bootctl_read_config(&boot);
    if (boot.slot[0].active) {
        boot.slot[1].bootable = true;
        boot.slot[1].active = true;
        boot.slot[0].active = false;
        BOOTCTL_LOG(LOG_INFO, "done slot a");
    } else {
        boot.slot[0].bootable = true;
        boot.slot[0].active = true;
        boot.slot[1].active = false;
        BOOTCTL_LOG(LOG_INFO, "done slot b");
    }

    return bootctl_write_config(&boot);
}

/* boot success, mark the slot successful, need run everytime */

int bootctl_success(void)
{
    struct bootctl_s boot;
    int ret = 0;

    bootctl_read_config(&boot);

    if (boot.slot[0].active) {
        if (boot.slot[0].successful == 0) {
            boot.try = false;
            boot.slot[0].successful = true;
            ret = bootctl_write_config(&boot);
            BOOTCTL_LOG(LOG_INFO, "success slot a");
        }
    } else {
        if (boot.slot[1].successful == 0) {
            boot.try = false;
            boot.slot[1].successful = true;
            ret = bootctl_write_config(&boot);
            BOOTCTL_LOG(LOG_INFO, "success slot b");
        }
    }

    return ret;
}
#else

/* boot to a active slot, if filed, just boot a successful slot */

static int bootctl_boot(void)
{
    struct boardioc_boot_info_s info;
    struct bootctl_s boot;

    boardctl(BOARDIOC_INIT, 0);
    bootctl_read_config(&boot);

    if (boot.slot[0].active == false && boot.slot[1].active == false) {
        memset(&boot, 0, sizeof(struct bootctl_s));
        boot.slot[0].active = true;
        boot.slot[0].bootable = true;
        boot.try = true;
    } else {
        if (boot.slot[0].active && boot.slot[0].bootable && boot.slot[0].successful == 0) {

            if (boot.try) {
                /* try filed, just boot a successful slot */
                boot.slot[0].active = false;
                boot.slot[0].bootable = false;
                BOOTCTL_LOG(LOG_INFO, "try boot slot a filed, boot to slot b");
                goto success_slot;
            }

            /* try boot slot 0 */
            info.path = g_bootctl_slot_a;
            boot.try = true;
            goto slot;
        } else if (boot.slot[1].active && boot.slot[1].bootable && boot.slot[1].successful == 0) {

            if (boot.try) {
                /* try filed, just boot a successful slot */
                boot.slot[1].active = false;
                boot.slot[1].bootable = false;
                BOOTCTL_LOG(LOG_INFO, "try boot slot b filed, boot to slot a");
                goto success_slot;
            }

            /* try boot slot b */

            info.path = g_bootctl_slot_b;
            boot.try = true;
            goto slot;
        }
    }

success_slot:
    if (boot.slot[0].active && boot.slot[0].successful) {
        /* boot slot 0, normal case */
        info.path = g_bootctl_slot_a;
    } else if (boot.slot[1].active && boot.slot[1].successful) {
        /* boot slot 1, normal case */
        info.path = g_bootctl_slot_b;
    } else if (boot.slot[0].successful) {
        /* last try boot 1 filed, so boot slot 0, */
        info.path = g_bootctl_slot_a;
        boot.slot[0].active = true;
    } else if (boot.slot[1].successful) {
        /* last try boot 0 filed, so boot slot 1, */
        info.path = g_bootctl_slot_b;
        boot.slot[1].active = true;
        boot.slot[1].bootable = true;
    } else {
        info.path = g_bootctl_slot_a;
        boot.slot[0].active = true;
        boot.slot[0].bootable = true;
    }

slot:
    bootctl_write_config(&boot);
    BOOTCTL_LOG(LOG_INFO, "boot to %s", info.path);
    boardctl(BOARDIOC_BOOT_IMAGE, (uintptr_t)&info);
    return 0;
}

#endif

int main(int argc, char* argv[])
{
#ifdef CONFIG_UTILS_BOOTCTL_ENTRY
    return bootctl_boot();
#else

    if (argc == 1) {
        bootctl_success();
    } else if (argc > 1) {
        if (strcmp(argv[1], "update") == 0) {
            return bootctl_update();
        } else if (strcmp(argv[1], "done") == 0) {
            return bootctl_done();
        } else if (strcmp(argv[1], "slot") == 0) {
            printf("run %s\n", bootctl_active());
        } else {
            return bootctl_success();
        }
    }
#endif
    return 0;
}
