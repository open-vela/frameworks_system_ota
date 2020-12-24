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

#include "recovery.h"

/****************************************************************************
 * recovery_main
 ****************************************************************************/

#ifdef CONFIG_LIB_LZMA
static SRes decode2(CLzmaDec *state, ISeqOutStream *outStream, ISeqInStream *inStream,
    UInt64 unpackSize)
{
  int thereIsSize = (unpackSize != (UInt64)(Int64)-1);
  Byte inBuf[IN_BUF_SIZE];
  Byte outBuf[OUT_BUF_SIZE];
  size_t inPos = 0, inSize = 0, outPos = 0;
  LzmaDec_Init(state);
  for (;;)
  {
    if (inPos == inSize)
    {
      inSize = IN_BUF_SIZE;
      RINOK(inStream->Read(inStream, inBuf, &inSize));
      inPos = 0;
    }
    {
      SRes ret;
      SizeT inProcessed = inSize - inPos;
      SizeT outProcessed = OUT_BUF_SIZE - outPos;
      ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
      ELzmaStatus status;
      if (thereIsSize && outProcessed > unpackSize)
      {
        outProcessed = (SizeT)unpackSize;
        finishMode = LZMA_FINISH_END;
      }

      ret = LzmaDec_DecodeToBuf(state, outBuf + outPos, &outProcessed,
        inBuf + inPos, &inProcessed, finishMode, &status);
      inPos += inProcessed;
      outPos += outProcessed;
      unpackSize -= outProcessed;

      if (outStream)
        if (outStream->Write(outStream, outBuf, outPos) != outPos)
          return SZ_ERROR_WRITE;

      outPos = 0;

      if (ret != SZ_OK || (thereIsSize && unpackSize == 0))
        return ret;

      if (inProcessed == 0 && outProcessed == 0)
      {
        if (thereIsSize || status != LZMA_STATUS_FINISHED_WITH_MARK)
          return SZ_ERROR_DATA;
        return ret;
      }
    }
  }
}

static SRes decode(ISeqOutStream *outStream, ISeqInStream *inStream)
{
  UInt64 unpackSize;
  int i;
  SRes ret = 0;

  CLzmaDec state;

  /* header: 5 bytes of LZMA properties and 8 bytes of uncompretsed size */
  unsigned char header[LZMA_PROPS_SIZE + 8];

  /* Read and parete header */

  RINOK(SeqInStream_Read(inStream, header, sizeof(header)));

  unpackSize = 0;
  for (i = 0; i < 8; i++)
    unpackSize += (UInt64)header[LZMA_PROPS_SIZE + i] << (i * 8);

  LzmaDec_Construct(&state);
  RINOK(LzmaDec_Allocate(&state, header, LZMA_PROPS_SIZE, &g_Alloc));
  ret = decode2(&state, outStream, inStream, unpackSize);
  LzmaDec_Free(&state, &g_Alloc);
  return ret;
}
#endif

int main(int argc, char *argv[])
{
  int write_fd;
  int ret;
  int normalboot = NORMAL_BOOT;

  printf("Recovery Service!\n");

#ifdef CONFIG_LIB_LZMA
  CFileSeqInStream inStream;
  CFileOutStream outStream;

  FileSeqInStream_CreateVTable(&inStream);
  File_Construct(&inStream.file);

  FileOutStream_CreateVTable(&outStream);
  File_Construct(&outStream.file);

  if (InFile_Open(&inStream.file, argv[1]) != 0) {
    printf("Can not open input file");
    return -1;
  }

  if (OutFile_Open(&outStream.file, argv[2]) != 0) {
    printf("Can not open output file");
    return -1;
  }

  ret = decode(&outStream.vt, &inStream.vt);
  if (ret != SZ_OK) {
    printf("Decode failed\n");
    if (ret == SZ_ERROR_MEM)
      printf("Decode failed, mem error\n");
    else if (ret == SZ_ERROR_DATA)
      printf("Decode failed, data error\n");
    else if (ret == SZ_ERROR_WRITE)
      printf("Decode failed, write error\n");
    else if (ret == SZ_ERROR_READ)
      printf("Decode failed, read error\n");
    else
      printf("Decode failed, unknown error\n");

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
    printf("open %s failed\n", APP_BIN);
    return -1;
  }

  //if ((write_fd = open(APP_DEV, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) < 0) {
  if ((write_fd = open(APP_DEV, O_RDWR)) < 0) {
    printf("open %s failed\n", APP_DEV);
    return -1;
  }

  if (fstat(read_fd, &sb) == -1) {
      printf("stat %s failed\n", APP_BIN);
      return -1;
  }

  file_len = sb.st_size;
  printf("%s size %d\n", APP_BIN, file_len);

  while (file_len > 0) {
    if ((len = read(read_fd, buf, MAX_SIZE)) < 0) {
      printf("read buf failed, ret %d\n", len);
      return -1;
    }

    if ((ret = write(write_fd, buf, len)) < 0) {
      printf("write buf failed, ret %d\n", ret);
      return -1;
    }

    file_len -= len;
  }

  close(read_fd);
  close(write_fd);

#endif

  /* Finally write the magic number for BES */
  if ((write_fd = open(APP_DEV, O_RDWR)) < 0) {
    printf("open %s failed\n", APP_DEV);
    return -1;
  }

  lseek(write_fd, 0, SEEK_SET);
  write(write_fd, &normalboot, 4);

  close(write_fd);

  printf("Recovery Service done, start rebooting to normal system!\n");
  system("reboot 0");

  return 0;
}
