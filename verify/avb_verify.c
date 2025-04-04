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

#include <errno.h>
#include <fcntl.h>
#include <libavb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "avb_verify.h"

static AvbIOResult read_from_partition(AvbOps* ops,
    const char* partition,
    int64_t offset,
    size_t num_bytes,
    void* buffer,
    size_t* out_num_read)
{
    size_t nread = 0;
    int fd;

    fd = open(partition, O_RDONLY);
    if (fd < 0)
        return AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;

    offset = lseek(fd, offset, offset >= 0 ? SEEK_SET : SEEK_END);
    if (offset < 0) {
        close(fd);
        return AVB_IO_RESULT_ERROR_RANGE_OUTSIDE_PARTITION;
    }

    while (num_bytes > 0) {
        ssize_t ret = read(fd, buffer, num_bytes);
        if (ret > 0) {
            nread += ret;
            buffer += ret;
            num_bytes -= ret;
        } else if (ret == 0 || errno != EINTR)
            break;
    }

    close(fd);
    if (num_bytes && nread == 0)
        return AVB_IO_RESULT_ERROR_IO;

    *out_num_read = nread;
    return AVB_IO_RESULT_OK;
}

static AvbIOResult get_preloaded_partition(AvbOps* ops,
    const char* partition,
    size_t num_bytes,
    uint8_t** out_pointer,
    size_t* out_num_bytes_preloaded)
{
    int fd;

    fd = open(partition, O_RDONLY);
    if (fd < 0)
        return AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;

    if (ioctl(fd, BIOC_XIPBASE, (uintptr_t)out_pointer) < 0)
        *out_pointer = NULL;

    close(fd);

    *out_num_bytes_preloaded = *out_pointer ? num_bytes : 0;
    return AVB_IO_RESULT_OK;
}

static AvbIOResult write_to_partition(AvbOps* ops,
    const char* partition,
    int64_t offset,
    size_t num_bytes,
    const void* buffer)
{
    int fd;

    fd = open(partition, O_WRONLY, 0660);
    if (fd < 0)
        return AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;

    offset = lseek(fd, offset, offset >= 0 ? SEEK_SET : SEEK_END);
    if (offset < 0) {
        close(fd);
        return AVB_IO_RESULT_ERROR_RANGE_OUTSIDE_PARTITION;
    }

    while (num_bytes > 0) {
        ssize_t ret = write(fd, buffer, num_bytes);
        if (ret > 0) {
            buffer += ret;
            num_bytes -= ret;
        } else if (ret == 0 || errno != EINTR)
            break;
    }

    close(fd);
    if (num_bytes)
        return AVB_IO_RESULT_ERROR_IO;

    return AVB_IO_RESULT_OK;
}

static AvbIOResult validate_vbmeta_public_key(AvbOps* ops,
    const uint8_t* public_key_data,
    size_t public_key_length,
    const uint8_t* public_key_metadata,
    size_t public_key_metadata_length,
    bool* out_is_trusted)
{
    return ops->validate_public_key_for_partition(ops,
        "vbmeta", public_key_data, public_key_length,
        public_key_metadata, public_key_metadata_length,
        out_is_trusted, NULL);
}

static AvbIOResult read_rollback_index(AvbOps* ops,
    size_t rollback_index_location,
    uint64_t* out_rollback_index)
{
    *out_rollback_index = 0;
    return AVB_IO_RESULT_OK;
}

static AvbIOResult read_is_device_unlocked(AvbOps* ops, bool* out_is_unlocked)
{
    *out_is_unlocked = false;
    return AVB_IO_RESULT_OK;
}

static AvbIOResult get_unique_guid_for_partition(AvbOps* ops,
    const char* partition,
    char* guid_buf,
    size_t guid_buf_size)
{
    memset(guid_buf, 0, guid_buf_size);
    strlcpy(guid_buf, partition, guid_buf_size);
    return AVB_IO_RESULT_OK;
}

static AvbIOResult get_size_of_partition(AvbOps* ops,
    const char* partition,
    uint64_t* out_size_num_bytes)
{
    struct stat buf;

    if (stat(partition, &buf) < 0)
        return AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;

    *out_size_num_bytes = buf.st_size;
    return AVB_IO_RESULT_OK;
}

static AvbIOResult validate_public_key_for_partition(AvbOps* ops,
    const char* partition,
    const uint8_t* public_key_data,
    size_t public_key_length,
    const uint8_t* public_key_metadata,
    size_t public_key_metadata_length,
    bool* out_is_trusted,
    uint32_t* out_rollback_index_location)
{
    AvbIOResult result;
    uint8_t* key_data;
    size_t key_length;

    key_data = calloc(1, public_key_length);
    if (key_data == NULL)
        return AVB_IO_RESULT_ERROR_OOM;

    result = ops->read_from_partition(ops,
        ops->user_data, 0, public_key_length, key_data, &key_length);
    if (result == AVB_IO_RESULT_OK) {
        *out_is_trusted = memcmp(key_data, public_key_data, public_key_length) == 0;
    }

    free(key_data);
    return result;
}

int avb_verify(struct avb_params_t* params)
{
    struct AvbOps ops = {
        (char*)params->key,
        NULL,
        NULL,
        read_from_partition,
        get_preloaded_partition,
        write_to_partition,
        validate_vbmeta_public_key,
        read_rollback_index,
        NULL,
        read_is_device_unlocked,
        get_unique_guid_for_partition,
        get_size_of_partition,
        NULL,
        NULL,
        validate_public_key_for_partition
    };
    const char* partitions[][2] = {
        { params->partition, NULL },
        { params->image, NULL },
    };
    AvbSlotVerifyData* slot_data[2] = { 0 };
    int ret;
    int n;

    for (n = 0; n < 2; n++) {
        ret = avb_slot_verify(&ops,
            partitions[n], params->suffix ? params->suffix : "",
            AVB_SLOT_VERIFY_FLAGS_NO_VBMETA_PARTITION,
            AVB_HASHTREE_ERROR_MODE_RESTART_AND_INVALIDATE,
            &slot_data[n]);

        if (!params->image)
            goto out;
    }

    if (!slot_data[0] || !slot_data[1])
        goto out;

    for (n = 0; n < AVB_MAX_NUMBER_OF_ROLLBACK_INDEX_LOCATIONS; n++) {
        if (slot_data[1]->rollback_indexes[n] < slot_data[0]->rollback_indexes[n]) {
            ret = AVB_SLOT_VERIFY_RESULT_ERROR_ROLLBACK_INDEX;
            goto out;
        }
    }

out:
    for (n = 0; n < 2; n++)
        if (slot_data[n])
            avb_slot_verify_data_free(slot_data[n]);
    return ret;
}

int avb_hash_desc(const char* full_partition_name, struct avb_hash_desc_t* desc)
{
    struct AvbOps ops = {
        NULL,
        NULL,
        NULL,
        read_from_partition,
        get_preloaded_partition,
        NULL,
        validate_vbmeta_public_key,
        NULL,
        NULL,
        read_is_device_unlocked,
        get_unique_guid_for_partition,
        get_size_of_partition,
        NULL,
        NULL,
        NULL
    };
    AvbFooter footer;
    size_t vbmeta_num_read;
    uint8_t* vbmeta_buf = NULL;
    size_t num_descriptors;
    const AvbDescriptor** descriptors;
    AvbDescriptor avb_desc;
    int ret;

    ret = avb_footer(&ops, full_partition_name, &footer);
    if (ret != AVB_IO_RESULT_OK) {
        avb_error("Loading footer failed: ", full_partition_name);
        return ret;
    }

    vbmeta_buf = avb_malloc(footer.vbmeta_size);
    if (vbmeta_buf == NULL) {
        return AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
    }

    ret = ops.read_from_partition(&ops,
        full_partition_name,
        footer.vbmeta_offset,
        footer.vbmeta_size,
        vbmeta_buf,
        &vbmeta_num_read);
    if (ret != AVB_IO_RESULT_OK) {
        goto out;
    }

    AvbVBMetaImageHeader vbmeta_header;
    avb_vbmeta_image_header_to_host_byte_order((AvbVBMetaImageHeader*)vbmeta_buf,
        &vbmeta_header);

    descriptors = avb_descriptor_get_all(vbmeta_buf, vbmeta_num_read, &num_descriptors);
    if (!avb_descriptor_validate_and_byteswap(descriptors[0], &avb_desc)) {
        avb_error(full_partition_name, ": Descriptor is invalid.\n");
        ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
        goto out;
    }

    switch (avb_desc.tag) {
    case AVB_DESCRIPTOR_TAG_HASH:
        AvbHashDescriptor avb_hash_desc;
        const AvbDescriptor* descriptor = descriptors[0];
        const uint8_t* desc_partition_name = NULL;
        const uint8_t* desc_salt;
        const uint8_t* desc_digest;

        if (!avb_hash_descriptor_validate_and_byteswap(
                (const AvbHashDescriptor*)descriptor, &avb_hash_desc)) {
            ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
            goto out;
        }
        desc_partition_name = ((const uint8_t*)descriptor) + sizeof(AvbHashDescriptor);
        desc_salt = desc_partition_name + avb_hash_desc.partition_name_len;
        desc_digest = desc_salt + avb_hash_desc.salt_len;
        if (avb_hash_desc.digest_len > sizeof(desc->digest)) {
            ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_ARGUMENT;
            goto out;
        }

        desc->digest_len = avb_hash_desc.digest_len;
        desc->image_size = avb_hash_desc.image_size;
        strlcpy((char*)desc->hash_algorithm, (char*)avb_hash_desc.hash_algorithm, sizeof(desc->hash_algorithm));
        memcpy(desc->digest, desc_digest, desc->digest_len);
        break;

    default:
        ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
        break;
    }

out:
    if (vbmeta_buf)
        avb_free(vbmeta_buf);
    return ret;
}

void avb_hash_desc_dump(const struct avb_hash_desc_t* desc)
{
    int i;

    avb_printf("%-16s : %" PRIu64 " bytes\n", "Image Size", desc->image_size);
    avb_printf("%-16s : %s\n", "Hash Algorithm", desc->hash_algorithm);
    avb_printf("%-16s : %" PRIu32 "\n", "Digest Length", desc->digest_len);
    avb_printf("%-16s : ", "Digest");
    for (i = 0; i < desc->digest_len; i++) {
        avb_printf("%02" PRIx8 "", desc->digest[i]);
    }
    avb_printf("\n");
}
