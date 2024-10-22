############################################################################
# frameworks/ota/Makefile
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

include $(APPDIR)/Make.defs

ifneq ($(CONFIG_OTA_UI),)
PROGNAME += otaUI
PRIORITY += $(CONFIG_OTA_UI_PRIORITY)
STACKSIZE += $(CONFIG_OTA_UI_STACKSIZE)
MODULE = $(CONFIG_OTA_UI)
CSRCS += ui/extra/lv_upgrade.c ui/ui_config_parse.c ui/ui_display.c
MAINSRC += ui/ui_ota.c
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/frameworks/system/utils
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/netutils/cjson/cJSON
endif

ifneq ($(CONFIG_UTILS_AVB_VERIFY),)
PROGNAME += $(CONFIG_UTILS_AVB_VERIFY_PROGNAME)
PRIORITY += $(CONFIG_UTILS_AVB_VERIFY_PRIORITY)
STACKSIZE += $(CONFIG_UTILS_AVB_VERIFY_STACKSIZE)
MODULE = $(CONFIG_UTILS_AVB_VERIFY)
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/external/avb/avb/libavb
MAINSRC += verify/avb_main.c
CSRCS += verify/avb_verify.c
endif

ifneq ($(CONFIG_UTILS_ZIP_VERIFY),)
PROGNAME += $(CONFIG_UTILS_ZIP_VERIFY_PROGNAME)
PRIORITY += $(CONFIG_UTILS_ZIP_VERIFY_PRIORITY)
STACKSIZE += $(CONFIG_UTILS_ZIP_VERIFY_STACKSIZE)
MODULE = $(CONFIG_UTILS_ZIP_VERIFY)
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/external/avb/avb
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/external/avb/avb/libavb
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/external/avb/avb/libavb/sha
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/system/zlib/zlib/contrib/minizip
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/system/zlib/zlib
CFLAGS += -DAVB_COMPILATION
MAINSRC += verify/zip_verify.c
endif

ifneq ($(CONFIG_UTILS_BOOTCTL),)
PROGNAME += $(CONFIG_UTILS_BOOTCTL_PROGNAME)
PRIORITY += $(CONFIG_UTILS_BOOTCTL_PRIORITY)
STACKSIZE += $(CONFIG_UTILS_BOOTCTL_STACKSIZE)
MODULE = $(CONFIG_UTILS_BOOTCTL)
MAINSRC += bootctl/bootctl.c
endif

include $(APPDIR)/Application.mk
