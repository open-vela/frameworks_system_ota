#ifndef _RECOVERY_H_
#define _RECOVERY_H_

#ifdef CONFIG_LIB_LZMA

#include "7zFile.h"
#include "7zVersion.h"
#include "Alloc.h"
#include "LzmaDec.h"

extern SRes decode(ISeqOutStream* outStream, ISeqInStream* inStream);

#endif

#define APP_BIN "/data/nuttx.bin"
#define APP_DEV "/dev/app"

#define MAX_SIZE 4096

/* Normal nuttx boot magic number */
#define BOOT_MAGIC_NUMBER 0xBE57EC1C
#define NORMAL_BOOT BOOT_MAGIC_NUMBER

#endif
