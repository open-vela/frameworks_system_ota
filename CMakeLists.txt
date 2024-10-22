#
# Copyright (C) 2021 Xiaomi Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#

if(CONFIG_OTA_UI)
  set(UI_CSRCS ui/ui_ota.c ui/extra/lv_upgrade.c ui/ui_config_parse.c
               ui/ui_display.c)
  set(UI_INCDIR ${NUTTX_APPS_DIR}/frameworks/system/utils/kvdb
                ${NUTTX_APPS_DIR}/netutils/cjson/cJSON)

  nuttx_add_application(
    MODULE
    ${CONFIG_OTA_UI}
    NAME
    otaUI
    STACKSIZE
    ${CONFIG_OTA_UI_PRIORITY}
    PRIORITY
    ${CONFIG_OTA_UI_PRIORITY}
    SRCS
    ${UI_CSRCS}
    INCLUDE_DIRECTORIES
    ${UI_INCDIR})
endif()

if(CONFIG_UTILS_AVB_VERIFY)
  set(AVB_VERIFY_CSRCS verify/avb_main.c verify/avb_verify.c)
  set(AVB_VERIFY_INCDIR ${NUTTX_APPS_DIR}/external/avb/avb/libavb)

  nuttx_add_application(
    MODULE
    ${CONFIG_UTILS_AVB_VERIFY}
    NAME
    ${CONFIG_UTILS_AVB_VERIFY_PROGNAME}
    STACKSIZE
    ${CONFIG_UTILS_AVB_VERIFY_STACKSIZE}
    PRIORITY
    ${CONFIG_UTILS_AVB_VERIFY_PRIORITY}
    SRCS
    ${AVB_VERIFY_CSRCS}
    INCLUDE_DIRECTORIES
    ${AVB_VERIFY_INCDIR})
endif()

if(CONFIG_UTILS_ZIP_VERIFY)
  set(ZIP_VERIFY_CSRCS verify/zip_verify.c)
  set(ZIP_VERIFY_INCDIR
      ${NUTTX_APPS_DIR}/external/avb/avb
      ${NUTTX_APPS_DIR}/external/avb/avb/libavb
      ${NUTTX_APPS_DIR}/external/avb/avb/libavb/sha
      ${NUTTX_APPS_DIR}/system/zlib/zlib/contrib/minizip
      ${NUTTX_APPS_DIR}/system/zlib/zlib)
  set(ZIP_VARIFY_CFLAGS -DAVB_COMPILATION)

  nuttx_add_application(
    MODULE
    ${CONFIG_UTILS_ZIP_VERIFY}
    NAME
    ${CONFIG_UTILS_ZIP_VERIFY_PROGNAME}
    STACKSIZE
    ${CONFIG_UTILS_ZIP_VERIFY_STACKSIZE}
    PRIORITY
    ${CONFIG_UTILS_ZIP_VERIFY_PRIORITY}
    SRCS
    ${ZIP_VERIFY_CSRCS}
    INCLUDE_DIRECTORIES
    ${ZIP_VERIFY_INCDIR}
    COMPILE_FLAGS
    ${ZIP_VARIFY_CFLAGS})
endif()

if(CONFIG_UTILS_BOOTCTL)
  nuttx_add_application(
    MODULE
    ${CONFIG_UTILS_BOOTCTL}
    NAME
    ${CONFIG_UTILS_BOOTCTL_PROGNAME}
    STACKSIZE
    ${CONFIG_UTILS_BOOTCTL_STACKSIZE}
    PRIORITY
    ${CONFIG_UTILS_BOOTCTL_PRIORITY}
    SRCS
    bootctl/bootctl.c)
endif()
