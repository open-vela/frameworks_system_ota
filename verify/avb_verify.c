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
#ifdef CONFIG_KVDB
#include <kvdb.h>
#endif
#include <libavb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "avb_verify.h"

#define AVB_PERSISTENT_VALUE "persist.%s"
#define AVB_DEVICE_UNLOCKED "persist.avb.unlocked"
#define AVB_ROLLBACK_LOCATION "persist.avb.rollback.%zu"

uint64_t g_rollback_index;

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

AvbIOResult validate_vbmeta_public_key(AvbOps* ops,
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
#ifdef CONFIG_UTILS_AVB_VERIFY_ENABLE_ROLLBACK_PROTECTION
    char key[PROP_NAME_MAX];

    snprintf(key, sizeof(key), AVB_ROLLBACK_LOCATION, rollback_index_location);
    *out_rollback_index = property_get_int64(key, 0);
#else
    *out_rollback_index = 0;
#endif
    return AVB_IO_RESULT_OK;
}

static AvbIOResult read_rollback_index_tmp(AvbOps* ops,
    size_t rollback_index_location,
    uint64_t* out_rollback_index)
{
    *out_rollback_index = g_rollback_index;
    return AVB_IO_RESULT_OK;
}

AvbIOResult write_rollback_index(AvbOps* ops,
    size_t rollback_index_location,
    uint64_t rollback_index)
{
#ifdef CONFIG_UTILS_AVB_VERIFY_ENABLE_ROLLBACK_PROTECTION
    char key[PROP_NAME_MAX];

    snprintf(key, sizeof(key), AVB_ROLLBACK_LOCATION, rollback_index_location);
    if (property_set_int64(key, rollback_index) < 0)
        return AVB_IO_RESULT_ERROR_IO;
#endif
    return AVB_IO_RESULT_OK;
}

AvbIOResult write_rollback_index_tmp(AvbOps* ops,
    size_t rollback_index_location,
    uint64_t rollback_index)
{
    g_rollback_index = rollback_index;
    return AVB_IO_RESULT_OK;
}

static AvbIOResult read_is_device_unlocked(AvbOps* ops, bool* out_is_unlocked)
{
#ifdef CONFIG_UTILS_AVB_VERIFY_ENABLE_DEVICE_LOCK
    *out_is_unlocked = property_get_bool(AVB_DEVICE_UNLOCKED, false);
#else
    *out_is_unlocked = false;
#endif
    return AVB_IO_RESULT_OK;
}

static AvbIOResult read_is_device_unlocked_false(AvbOps* ops, bool* out_is_unlocked)
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

static AvbIOResult read_persistent_value(AvbOps* ops,
    const char* name,
    size_t buffer_size,
    uint8_t* out_buffer,
    size_t* out_num_bytes_read)
{
#ifdef CONFIG_UTILS_AVB_VERIFY_ENABLE_PERSISTENT_VALUE
    ssize_t ret;
    char key[PROP_NAME_MAX];

    snprintf(key, sizeof(key), AVB_PERSISTENT_VALUE, name);
    ret = property_get_buffer(key, out_buffer, buffer_size);
    if (ret == -E2BIG) {
        *out_num_bytes_read = PROP_VALUE_MAX;
        return AVB_IO_RESULT_ERROR_INSUFFICIENT_SPACE;
    } else if (ret < 0)
        return AVB_IO_RESULT_ERROR_NO_SUCH_VALUE;

    *out_num_bytes_read = ret;
#else
    *out_num_bytes_read = 0;
#endif
    return AVB_IO_RESULT_OK;
}

static AvbIOResult write_persistent_value(AvbOps* ops,
    const char* name,
    size_t value_size,
    const uint8_t* value)
{
#ifdef CONFIG_UTILS_AVB_VERIFY_ENABLE_PERSISTENT_VALUE
    char key[PROP_NAME_MAX];

    snprintf(key, sizeof(key), AVB_PERSISTENT_VALUE, name);
    if (property_set_buffer(key, value, value_size) < 0)
        return AVB_IO_RESULT_ERROR_INVALID_VALUE_SIZE;
#endif
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

int avb_verify(const char* partition, const char* key, const char* suffix, AvbSlotVerifyFlags flags)
{
    struct AvbOps ops = {
        (char*)key,
        NULL,
        NULL,
        read_from_partition,
        get_preloaded_partition,
        write_to_partition,
        validate_vbmeta_public_key,
        (flags & UTILS_AVB_VERIFY_LOCAL_FLAG_NOKV) ? read_rollback_index_tmp : read_rollback_index,
        (flags & UTILS_AVB_VERIFY_LOCAL_FLAG_NOKV) ? write_rollback_index_tmp : write_rollback_index,
        (flags & UTILS_AVB_VERIFY_LOCAL_FLAG_NOKV) ? read_is_device_unlocked_false : read_is_device_unlocked,
        get_unique_guid_for_partition,
        get_size_of_partition,
        (flags & UTILS_AVB_VERIFY_LOCAL_FLAG_NOKV) ? NULL : read_persistent_value,
        (flags & UTILS_AVB_VERIFY_LOCAL_FLAG_NOKV) ? NULL : write_persistent_value,
        validate_public_key_for_partition
    };
    const char* partitions[] = {
        partition,
        NULL
    };
    AvbSlotVerifyData* slot_data = NULL;
    int ret;
    int n;

    ret = avb_slot_verify(&ops,
        partitions, suffix ? suffix : "",
        (flags & ~UTILS_AVB_VERIFY_LOCAL_FLAG_MASK) | AVB_SLOT_VERIFY_FLAGS_NO_VBMETA_PARTITION,
        AVB_HASHTREE_ERROR_MODE_RESTART_AND_INVALIDATE,
        &slot_data);

    if (ret != AVB_SLOT_VERIFY_RESULT_OK || !slot_data)
        return ret;

    for (n = 0; n < AVB_MAX_NUMBER_OF_ROLLBACK_INDEX_LOCATIONS; n++) {
        uint64_t rollback_index = slot_data->rollback_indexes[n];
        if (rollback_index) {
            uint64_t current_rollback_index;
            ret = ops.read_rollback_index(&ops, n, &current_rollback_index);
            if (ret != AVB_IO_RESULT_OK)
                goto out;
            if (current_rollback_index != rollback_index && (flags & AVB_SLOT_VERIFY_FLAGS_NOT_UPDATE_ROLLBACK_INDEX) == 0) {
                ret = ops.write_rollback_index(&ops, n, rollback_index);
                if (ret != AVB_IO_RESULT_OK)
                    goto out;
            }
        }
    }

out:
    avb_slot_verify_data_free(slot_data);
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
        read_rollback_index,
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
