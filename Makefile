############################################################################
# frameworks/ota/recovery/Makefile
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


ifneq ($(CONFIG_SERVICES_RECOVERY),)

PROGNAME  = $(CONFIG_SERVICES_RECOVERY_PROGNAME)
PRIORITY  = $(CONFIG_SERVICES_RECOVERY_PRIORITY)
STACKSIZE = $(CONFIG_SERVICES_RECOVERY_STACKSIZE)
MODULE    = $(CONFIG_SERVICES_RECOVERY)

ifeq ($(CONFIG_LIB_LZMA),y)
CSRCS += recovery/decode.c
endif

ifeq ($(CONFIG_LIB_MBEDTLS),y)
CSRCS += recovery/verify.c
endif

MAINSRC += recovery/recovery.c

CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/lzma/C}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/mbedtls/include}

endif

ifneq ($(CONFIG_VELA_VERIFY),)

PROGNAME = $(CONFIG_VELA_VERIFY_PROGNAME)
PRIORITY = $(CONFIG_VELA_VERIFY_PRIORITY)
STACKSIZE = $(CONFIG_VELA_VERIFY_STACKSIZE)
MODULE = $(CONFIG_VELA_VERIFY)

CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/zlib/contrib/minizip}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/zlib}
MAINSRC += tools/verify/verify.c

endif

include $(APPDIR)/Application.mk
