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

ifneq ($(CONFIG_OTA_VERIFY),)
PROGNAME = $(CONFIG_OTA_VERIFY_PROGNAME)
PRIORITY = $(CONFIG_OTA_VERIFY_PRIORITY)
STACKSIZE = $(CONFIG_OTA_VERIFY_STACKSIZE)
MODULE = $(CONFIG_OTA_VERIFY)

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/external/zlib/zlib/contrib/minizip
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/external/zlib/zlib
MAINSRC += verify/verify.c

endif

ifneq ($(CONFIG_OTA_UI),)
PROGNAME += otaUI
PRIORITY += SCHED_PRIORITY_DEFAULT
STACKSIZE += $(CONFIG_DEFAULT_TASK_STACKSIZE)
MODULE = $(CONFIG_OTA_UI)
CSRCS += ui/extra/lv_upgrade.c ui/ui_config_parse.c ui/ui_display.c
MAINSRC += ui/ui_ota.c
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/frameworks/kvdb}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/netutils/cjson/cJSON}
endif

include $(APPDIR)/Application.mk

