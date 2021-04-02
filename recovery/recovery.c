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

#include <fcntl.h>
#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

#include "recovery.h"
#include "velaimg.h"

/****************************************************************************
 * recovery_main
 ****************************************************************************/

int main(int argc, char* argv[])
{
    int write_fd;
    int ret;
    int normalboot = NORMAL_BOOT;
    struct stat sb;

    syslog(LOG_INFO, "Recovery Service start!\n");

#ifdef CONFIG_LIB_MBEDTLS
    FILE* f;
    vela_img_hdr header;
    char lzma_header[LZMA_PROPS_SIZE + 8];
    int raw_image_size = 0;
    char* image;
    char signature[256];

    if ((f = fopen(argv[1], "rb")) == NULL) {
        syslog(LOG_ERR, "Could not open %s\n\n", argv[1]);
        ret = -1;
        goto exit;
    }

    ret = fread(&header, 1, sizeof(header), f);
    if (ret != sizeof(header)) {
        syslog(LOG_INFO, "read header failed\n");
        ret = -1;
        goto cleanup_file;
    }

    /* Compare header magic */
    if (memcmp(header.magic, VELA_MAGIC, VELA_MAGIC_SIZE)) {
        syslog(LOG_ERR, "Invalid image magic\n");
        ret = -1;
        goto cleanup_file;
    }

    /* Compare uncompressed raw image size to app partition size */
    if (stat(argv[2], &sb) == -1) {
        syslog(LOG_ERR, "stat %s failed\n", argv[2]);
        ret = -1;
        goto cleanup_file;
    }

    ret = fread(lzma_header, 1, sizeof(lzma_header), f);
    if (ret != sizeof(lzma_header)) {
        syslog(LOG_INFO, "read lzma header failed\n");
        ret = -1;
        goto cleanup_file;
    }

    for (int i = 0; i < 8; i++)
      raw_image_size += lzma_header[LZMA_PROPS_SIZE + i] << (i * 8);

    if (raw_image_size > sb.st_size) {
        syslog(LOG_ERR, "image size %ld larger than app parititon size %ld\n", raw_image_size, sb.st_size);
        ret = -1;
        goto cleanup_file;
    }

    syslog(LOG_INFO, "vela header: magic %s, image size %d, signature size %d\n",
        header.magic, header.image_size, header.sign_size);

    image = malloc(header.image_size);
    if (!image) {
        syslog(LOG_INFO, "malloc failed\n");
        ret = -1;
        goto cleanup_file;
    }

    fseek(f, -sizeof(lzma_header), SEEK_CUR);

    ret = fread(image, 1, header.image_size, f);
    if (ret != header.image_size) {
        syslog(LOG_INFO, "read raw image failed\n");
        ret = -1;
        goto cleanup_image;
    }

    ret = fread(signature, 1, header.sign_size, f);
    if (ret != header.sign_size) {
        syslog(LOG_INFO, "read signature failed\n");
        ret = -1;
        goto cleanup_image;
    }

    ret = verify_vela_image(&header, image, signature);
    if (ret) {
        syslog(LOG_ERR, "Verify vela image failed\n");
    }

cleanup_image:
    free(image);
cleanup_file:
    fclose(f);

    if (ret) {
        /* Remove image since image verified error */
        syslog(LOG_INFO, "Remove %s since image incomplete\n", argv[1]);
        if (unlink(argv[1]) == -1)
            syslog(LOG_ERR, "Remove %s failed\n", argv[1]);

        goto exit;
    }

#endif

#ifdef CONFIG_LIB_LZMA

    CFileSeqInStream inStream;
    CFileOutStream outStream;

    FileSeqInStream_CreateVTable(&inStream);
    File_Construct(&inStream.file);

    FileOutStream_CreateVTable(&outStream);
    File_Construct(&outStream.file);

    if (InFile_Open(&inStream.file, argv[1]) != 0) {
        syslog(LOG_ERR, "Can not open input file %s\n", argv[1]);
        ret = -1;
        goto exit;
    }

    if (OutFile_Open(&outStream.file, argv[2]) != 0) {
        syslog(LOG_ERR, "Can not open output file %s\n", argv[2]);
        ret = -1;
        goto cleanup_infile;
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

        ret = -1;
    }

    File_Close(&outStream.file);
cleanup_infile:
    File_Close(&inStream.file);

    if (ret)
      goto exit;

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
        syslog(LOG_ERR, "open %s to write magic number failed\n", APP_DEV);
        ret = -1;
        goto exit;
    }

    lseek(write_fd, 0, SEEK_SET);
    write(write_fd, &normalboot, 4);

    close(write_fd);

exit:
    if (ret)
        syslog(LOG_ERR, "Recovery failed, rebooting to old normal system\n");
    else {
        syslog(LOG_INFO, "Recovery succeeded, rebooting to new normal system!\n");

        /* Remove image since recovery succeed */
        if (unlink(argv[1]) == -1)
            syslog(LOG_ERR, "Remove %s failed\n", argv[1]);
    }

    system("reboot 0");

    return ret;
}
