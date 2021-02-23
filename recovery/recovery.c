/****************************************************************************
 * services/recovery/recovery.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <syslog.h>

#include "recovery.h"

/****************************************************************************
 * recovery_main
 ****************************************************************************/

int main(int argc, char *argv[])
{
  int write_fd;
  int ret;
  int normalboot = NORMAL_BOOT;

  syslog(LOG_INFO, "Recovery Service start!\n");

#ifdef CONFIG_LIB_LZMA

  CFileSeqInStream inStream;
  CFileOutStream outStream;

  FileSeqInStream_CreateVTable(&inStream);
  File_Construct(&inStream.file);

  FileOutStream_CreateVTable(&outStream);
  File_Construct(&outStream.file);

  if (InFile_Open(&inStream.file, argv[1]) != 0) {
    syslog(LOG_ERR, "Can not open input file %s\n", argv[1]);
    return -1;
  }

  if (OutFile_Open(&outStream.file, argv[2]) != 0) {
    syslog(LOG_ERR, "Can not open output file %s\n", argv[2]);
    return -1;
  }

  ret = decode(&outStream.vt, &inStream.vt);
  if (ret != SZ_OK) {
    syslog(LOG_ERR, "Decode failed\n");
    if (ret == SZ_ERROR_MEM)
      syslog(LOG_ERR, "Decode failed, mem error\n");
    else if (ret == SZ_ERROR_DATA)
      syslog(LOG_ERR, "Decode failed, data error\n");
    else if (ret == SZ_ERROR_WRITE)
      syslog(LOG_ERR, "Decode failed, write error\n");
    else if (ret == SZ_ERROR_READ)
      syslog(LOG_ERR, "Decode failed, read error\n");
    else
      syslog(LOG_ERR, "Decode failed, unknown error\n");

    return -1;
  }

  File_Close(&outStream.file);
  File_Close(&inStream.file);

#else

  int read_fd;
  int file_len;
  int len;
  char buf[MAX_SIZE];
  struct stat sb;

  if ((read_fd = open(APP_BIN, O_RDONLY)) < 0) {
    syslog(LOG_ERR, "open %s failed\n", APP_BIN);
    return -1;
  }

  //if ((write_fd = open(APP_DEV, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) < 0) {
  if ((write_fd = open(APP_DEV, O_RDWR)) < 0) {
    syslog(LOG_ERR, "open %s failed\n", APP_DEV);
    return -1;
  }

  if (fstat(read_fd, &sb) == -1) {
      syslog(LOG_ERR, "stat %s failed\n", APP_BIN);
      return -1;
  }

  file_len = sb.st_size;

  while (file_len > 0) {
    if ((len = read(read_fd, buf, MAX_SIZE)) < 0) {
      syslog(LOG_ERR, "read buf failed, ret %d\n", len);
      return -1;
    }

    if ((ret = write(write_fd, buf, len)) < 0) {
      syslog(LOG_ERR, "write buf failed, ret %d\n", ret);
      return -1;
    }

    file_len -= len;
  }

  close(read_fd);
  close(write_fd);

#endif

  /* Finally write the magic number for BES */
  if ((write_fd = open(APP_DEV, O_RDWR)) < 0) {
    syslog(LOG_ERR, "open %s failed\n", APP_DEV);
    return -1;
  }

  lseek(write_fd, 0, SEEK_SET);
  write(write_fd, &normalboot, 4);

  close(write_fd);

  syslog(LOG_INFO, "Recovery Service done, start rebooting to normal system!\n");
  system("reboot 0");

  return 0;
}
