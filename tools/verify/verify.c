/****************************************************************************
 * frameworks/ota/tools/verify/verify.c
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

#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <unzip.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define DIGESTED_CHUNK_MAX_SIZE (1024 * 1024)

#define assert_res(x, ...)                                                          \
    do {                                                                            \
        if (!(x)) {                                                                 \
            if (strlen(#__VA_ARGS__) != 0) {                                        \
                syslog(LOG_ERR, "%s:%d >> %s\n", __FILE__, __LINE__, #__VA_ARGS__); \
            }                                                                       \
            goto error;                                                             \
        }                                                                           \
    } while (0)

typedef enum {
    VERIFY_OK,
    VERIFY_UNSIGNED,
    VERIFY_FORMAT_ERROR,
    VERIFY_FILE_NOT_EXIT
} VERIFY_ERROR;

// length - data block
typedef struct data_block_s {
    uint8_t* data;
    uint32_t length;
} data_block_t;

typedef struct app_block_s {
    data_block_t data_block;
    data_block_t signature_block;
    data_block_t central_directory_block;
    data_block_t eocd_block;
} app_block_t;

// Single signature block data structure
/*---------------------

    signature_block_s
    |
    |---- signed_data
    |     |---- digests
    |     |---- certificate
    |
    |---- signatures
    |     |---- signatures_algorithm_id
    |     |---- signatures_content
    |
    |---- public_key

------------------------*/

typedef struct signature_block_s {
    data_block_t one_sign_block;
    data_block_t signed_data;
    data_block_t signatures;
    data_block_t public_key;
    data_block_t digests;
    uint32_t digests_signatures_algorithm_id;
    data_block_t one_digest;
    data_block_t certificate;
    uint32_t signatures_algorithm_id;
    data_block_t signatures_content;
} signature_block_t;

static int calc_chunk_count(data_block_t* block);

/**
 * @brief parse len-data block
 */
static uint8_t* parse_block(uint8_t* data, data_block_t* block)
{
    uint32_t len;
    assert(data && block);

    uint8_t* offset = data;

    memcpy(&len, data, sizeof(uint32_t));

    block->length = len;
    offset += 4;
    block->data = offset;
    offset += block->length;

    return offset;
}

/**
 * @brief parse len-id-data block
 */
static uint8_t* parse_kv_block(uint8_t* data, uint64_t* key, uint8_t** value)
{
    assert(data && key);

    uint8_t* offset = data;
    uint64_t length = 0;
    memcpy(&length, data, sizeof(uint64_t));

    offset += 8;
    memcpy(key, offset, sizeof(uint32_t));
    offset += 4;
    *value = offset;
    offset += length - 4;

    return offset;
}

/**
 * @brief Get all block data of app
 */
static app_block_t* parse_app_block(const char* app_path, size_t comment_len)
{
    int fd = -1;
    int res = -1;
    off_t central_directory_offset = 0;
    char magic_buf[16] = { 0 };
    const char* magic = "APK Sig Block 42";
    app_block_t* app_block = malloc(sizeof(app_block_t));
    assert_res(app_block != NULL);

    fd = open(app_path, O_RDONLY);
    assert_res(fd > 0);

    // Get Central_ Directory start offset
    off_t central_directory_offset_ptr = -(off_t)comment_len - 4 - 2;
    lseek(fd, central_directory_offset_ptr, SEEK_END);
    res = read(fd, &central_directory_offset, 4);
    assert_res(res > 0);
    app_block->central_directory_block.data = (uint8_t*)(uintptr_t)central_directory_offset;

    // format check
    lseek(fd, central_directory_offset - 16, SEEK_SET);
    res = read(fd, &magic_buf, sizeof(magic_buf));
    assert_res(res > 0);
    res = memcmp(magic, magic_buf, sizeof(magic_buf));
    assert_res(res == 0);

    // Get signature block length
    off_t signature_block_len_offset = central_directory_offset - 16 - 8, signature_block_length = 0;
    lseek(fd, signature_block_len_offset, SEEK_SET);
    res = read(fd, &signature_block_length, 4);
    assert_res(res > 0);
    app_block->signature_block.length = signature_block_length + 8;

    // Read signature block
    app_block->signature_block.data = (uint8_t*)(uintptr_t)(central_directory_offset - app_block->signature_block.length);

    // Read EOCD
    off_t ecod_start_offset = central_directory_offset_ptr - 16;
    lseek(fd, ecod_start_offset, SEEK_END);
    app_block->eocd_block.length = -1 * ecod_start_offset;
    ecod_start_offset = lseek(fd, 0, SEEK_CUR);
    app_block->eocd_block.data = (uint8_t*)(uintptr_t)ecod_start_offset;

    // Read central directory
    app_block->central_directory_block.length = ecod_start_offset - central_directory_offset;

    // Read zip content data block
    app_block->data_block.data = 0;
    app_block->data_block.length = (uintptr_t)app_block->signature_block.data;

    close(fd);
    return app_block;
error:
    res = -1;
    close(fd);
    free(app_block);
    return NULL;
}

/**
 * @brief Get signature block information
 */
static uint8_t* get_signature_info(uint8_t* data, signature_block_t* info)
{
    uint8_t *offset, *ret;

    // Get current signature block
    offset = parse_block(data, &info->one_sign_block);

    // Get current signature data block
    offset = parse_block(info->one_sign_block.data, &info->signed_data);
    offset = parse_block(offset, &info->signatures);
    offset = parse_block(offset, &info->public_key);
    ret = offset;

    // Get signed_ Data data
    offset = parse_block(info->signed_data.data, &info->digests);
    memcpy(&info->digests_signatures_algorithm_id, (info->digests.data + 4), sizeof(uint32_t));
    offset = parse_block(info->digests.data + 8, &info->one_digest);

    offset = parse_block(offset, &info->certificate);
    offset = parse_block(info->certificate.data, &info->certificate);

    // Get signatures data
    offset = parse_block(info->signatures.data, &info->signatures_content);
    memcpy(&info->signatures_algorithm_id, info->signatures_content.data, sizeof(uint32_t));
    offset = parse_block(info->signatures.data + 8, &info->signatures_content);

    return ret;
}

/**
 * @brief verify app signature block
 */
static int verify_block_signature(data_block_t* pubkey, data_block_t* raw_data, data_block_t* signature)
{
    int res;
    unsigned char* md = NULL;
    mbedtls_pk_context pk;

    // Initialize public key
    mbedtls_pk_init(&pk);
    res = mbedtls_pk_parse_public_key(&pk, pubkey->data, pubkey->length);
    assert_res(res == 0);

    // Specify the digest calculation method
    const mbedtls_md_info_t* mdinfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    md = malloc(mbedtls_md_get_size(mdinfo));
    res = -1;
    assert_res(md != NULL);

    // Validation data digest
    res = mbedtls_md(mdinfo, raw_data->data, raw_data->length, md);
    assert_res(res == 0);

    // Verify signature
    res = mbedtls_pk_verify(&pk, mbedtls_md_get_type(mdinfo),
            md, mbedtls_md_get_size(mdinfo), signature->data, signature->length);
    assert_res(res == 0);

error:
    free(md);
    mbedtls_pk_free(&pk);
    return res;
}

static int md_file_block(int fd, data_block_t* block, unsigned char* output)
{
    int res;
    size_t n, sum = 0;
    mbedtls_md_context_t ctx;
    unsigned char buf[1024], prefix = 0xa5;
    const mbedtls_md_info_t* mdinfo;

    mdinfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_init(&ctx);

    res = mbedtls_md_setup(&ctx, mdinfo, 0);
    assert_res(res == 0);

    res = mbedtls_md_starts(&ctx);
    assert_res(res == 0);

    res = mbedtls_md_update(&ctx, (const unsigned char*)&prefix, 1);
    assert_res(res == 0);
    res = mbedtls_md_update(&ctx, (const unsigned char*)&block->length, sizeof(uint32_t));
    assert_res(res == 0);
    lseek(fd, (uintptr_t)block->data, SEEK_SET);

    while (sum < block->length) {
        size_t read_len = (block->length - sum) % sizeof(buf);
        read_len = (read_len == 0) ? sizeof(buf) : read_len;
        n = read(fd, buf, read_len);
        assert_res(n > 0);
        sum += n;

        res = mbedtls_md_update(&ctx, buf, n);
        assert_res(res == 0);
    }

    res = mbedtls_md_finish(&ctx, output);
    assert_res(res == 0);

error:
    mbedtls_md_free(&ctx);
    return res;
}

static int md_rpk_block(mbedtls_md_context_t* rpk_ctx, int fd, data_block_t* block)
{
    int res;
    data_block_t sub_chunk;
    ssize_t lenght = block->length;
    unsigned char md[32];

    int cnt = calc_chunk_count(block);
    for (int i = 0; i < cnt; i++) {
        sub_chunk.data = block->data + i * DIGESTED_CHUNK_MAX_SIZE;
        if (lenght - DIGESTED_CHUNK_MAX_SIZE > 0) {
            sub_chunk.length = DIGESTED_CHUNK_MAX_SIZE;
            lenght -= DIGESTED_CHUNK_MAX_SIZE;
        } else {
            sub_chunk.length = lenght;
        }

        res = md_file_block(fd, &sub_chunk, md);
        assert_res(res == 0);
        res = mbedtls_md_update(rpk_ctx, md, sizeof(md));
        assert_res(res == 0);
    }

    return 0;
error:
    return -1;
}

static int calc_chunk_count(data_block_t* block)
{
    return (block->length + DIGESTED_CHUNK_MAX_SIZE - 1) / DIGESTED_CHUNK_MAX_SIZE;
}

/**
 * @brief Verify the app Digest
 */
static int app_block_digest_verification(const char* path, app_block_t* app_block, data_block_t* digest)
{
    int res = -1;
    int chunk_count = 0;
    unsigned char md[32], *buff = NULL, prefix = 0xa5;
    const mbedtls_md_info_t* mdinfo;
    mbedtls_md_context_t block_ctx = {0}, rpk_ctx = {0};

    int fd = open(path, O_RDONLY);
    assert_res(fd > 0);

    mbedtls_md_init(&rpk_ctx);
    mdinfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    res = mbedtls_md_setup(&rpk_ctx, mdinfo, 0);
    assert_res(res == 0);
    res = mbedtls_md_setup(&block_ctx, mdinfo, 0);
    assert_res(res == 0);

    mbedtls_md_starts(&rpk_ctx);
    prefix = 0x5a;

    chunk_count += calc_chunk_count(&app_block->data_block);
    chunk_count += calc_chunk_count(&app_block->central_directory_block);
    chunk_count += calc_chunk_count(&app_block->eocd_block);

    res = mbedtls_md_update(&rpk_ctx, &prefix, 1);
    assert_res(res == 0);
    res = mbedtls_md_update(&rpk_ctx, (const unsigned char *)&chunk_count, sizeof(chunk_count));
    assert_res(res == 0);

    md_rpk_block(&rpk_ctx, fd, &app_block->data_block);
    md_rpk_block(&rpk_ctx, fd, &app_block->central_directory_block);

    // Modify central directory offset
    prefix = 0xa5;
    buff = malloc(app_block->eocd_block.length);
    assert_res(buff != NULL);
    mbedtls_md_starts(&block_ctx);
    lseek(fd, (uintptr_t)app_block->eocd_block.data, SEEK_SET);
    read(fd, buff, app_block->eocd_block.length);
    memcpy(buff + 16, &app_block->data_block.length, sizeof(uint32_t));
    mbedtls_md_update(&block_ctx, (const unsigned char*)&prefix, 1);
    mbedtls_md_update(&block_ctx, (const unsigned char*)&app_block->eocd_block.length, sizeof(uint32_t));
    mbedtls_md_update(&block_ctx, (const unsigned char*)buff, app_block->eocd_block.length);
    mbedtls_md_finish(&block_ctx, md);
    mbedtls_md_update(&rpk_ctx, md, sizeof(md));

    mbedtls_md_finish(&rpk_ctx, md);

    mbedtls_md_free(&rpk_ctx);
    mbedtls_md_free(&block_ctx);

    res = memcmp(digest->data, md, sizeof(md));
    assert_res(res == 0);

error:
    free(buff);
    close(fd);
    return res;
}

int certificate_verify(signature_block_t* certificate, const char* path)
{

    int fd = -1, res = -1;
    char* buff = NULL;
    struct stat st;
    data_block_t cert;

    assert_res(certificate != NULL);
    assert_res(path != NULL);

    res = access(path, F_OK);
    assert_res(res == 0);

    res = stat(path, &st);
    assert_res(res == 0);
    cert = certificate->certificate;

    res = -1;
    assert_res(st.st_size == cert.length);

    res = -1;
    buff = malloc(cert.length);
    assert_res(buff != NULL);

    res = -1;
    fd = open(path, O_RDONLY);
    assert_res(fd > 0);
    res = read(fd, buff, certificate->certificate.length);
    assert_res(res > 0);

    res = memcmp(cert.data, buff, cert.length);
    assert_res(res == 0);

error:
    free(buff);
    close(fd);
    return res;
}

/**
 * @brief app Signature verification
 */
static int app_verification(const char* app_path, const char* cert_path, size_t comment_len)
{
    int res = -1, fd = -1;
    uint64_t id;
    uint8_t* offset;
    app_block_t* app_block = NULL;
    char* signature_block_data = NULL;
    data_block_t signature_data;
    signature_block_t signature_info;

    // get APK Signing Block
    app_block = parse_app_block(app_path, comment_len);
    assert_res(app_block != NULL);

    // parse Signing Block
    signature_block_data = malloc(app_block->signature_block.length);
    assert_res(signature_block_data != NULL);
    res = -1;
    fd = open(app_path, O_RDONLY);
    assert_res(fd > 0);
    lseek(fd, (uintptr_t)app_block->signature_block.data, SEEK_SET);
    read(fd, signature_block_data, app_block->signature_block.length);

    parse_kv_block((uint8_t*)(signature_block_data + 8), &id, &offset);
    parse_block(offset, &signature_data);
    get_signature_info(signature_data.data, &signature_info);

    // Verify block signature
    res = verify_block_signature(&signature_info.public_key, &signature_info.signed_data,
        &signature_info.signatures_content);
    assert_res(res == 0);

    // Compare whether the signature algorithms are consistent
    res = -1;
    assert_res(signature_info.digests_signatures_algorithm_id == signature_info.signatures_algorithm_id);

    //  Compare whether the app summary is consistent with the signature block summary
    res = app_block_digest_verification(app_path, app_block, &signature_info.one_digest);
    assert_res(res == 0);

    res = certificate_verify(&signature_info, cert_path);
    assert_res(res == 0);

error:
    close(fd);
    free(signature_block_data);
    free(app_block);
    return res;
}

static int verify(const char* app_path, const char *cert_path)
{
    unzFile zFile;
    unz_global_info64 zGlobalInfo;
    int res = -1;

    // open file
    assert_res(app_path);
    zFile = unzOpen(app_path);
    assert_res(zFile != NULL, "file format error");
    res = unzGetGlobalInfo64(zFile, &zGlobalInfo);
    assert_res(res == UNZ_OK);
    res = unzClose(zFile);
    assert_res(res == UNZ_OK);

    // Verify app legitimacy
    res = app_verification(app_path, cert_path, zGlobalInfo.size_comment);
    assert_res(res == 0);

error:
    return res;
}

int main(int argc, char* argv[])
{
    int res = 0;
    const char* file;
    const char* cert;

    if (argc != 3) {
        printf("verify: missing required argument\n");
        printf("Usage: verify <file> <cert>\n");
        return -EINVAL;
    }

    file = argv[1];
    res = access(file, F_OK);
    assert_res((0 == res), "File not found");

    cert = argv[2];
    res = access(cert, F_OK);
    assert_res((0 == res), "Cert not found");

    res = verify(file, cert);
    assert_res((0 == res), "File verify failed");

error:
    return res;
}
