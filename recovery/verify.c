/****************************************************************************
 * services/recovery/verify.c
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

#include <syslog.h>
#include "mbedtls/md.h"
#include "mbedtls/pk.h"

#include "velaimg.h"

static int verify_vela_image_hash(vela_img_hdr* hdr, char* image)
{
    int ret = -1;
    mbedtls_md_context_t ctx;
    char sha[32];

    if (memcmp(hdr->magic, VELA_MAGIC, VELA_MAGIC_SIZE)) {
        syslog(LOG_ERR, "ERROR: Invalid image magic\n");
        return -1;
    }

    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL)
        return -1;

    mbedtls_md_init(&ctx);
    ret = mbedtls_md_setup(&ctx, md_info, 0);
    if (ret != 0)
        goto cleanup;

    ret = mbedtls_md_starts(&ctx);
    if (ret != 0)
        goto cleanup;
    ret = mbedtls_md_update(&ctx, image, hdr->image_size);
    if (ret != 0)
        goto cleanup;
    ret = mbedtls_md_finish(&ctx, sha);

    if (memcmp(hdr->hash, sha, sizeof(hdr->hash))) {
        syslog(LOG_ERR, "ERROR: Fail to verify image hash\n");
        ret = -1;
    }

cleanup:
    mbedtls_md_free(&ctx);
    return ret;
}

static int verify_vela_image_signature(vela_img_hdr* hdr, char* signature)
{
    int ret;
    mbedtls_pk_context pk;

    mbedtls_pk_init(&pk);

    if ((ret = mbedtls_pk_parse_public_keyfile(&pk, "/etc/public.key")) != 0) {
        syslog(LOG_ERR, "ERROR: public keyfile parse failed!\n");
        goto exit;
    }

    if ((ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hdr->hash, 0,
             signature, hdr->sign_size))
        != 0) {
        syslog(LOG_INFO, "ERROR: pubkey verify failed!\n");
        goto exit;
    }

exit:
    mbedtls_pk_free(&pk);
    return ret;
}

int verify_vela_image(vela_img_hdr* hdr, char* image, char* signature)
{
    int ret;

    ret = verify_vela_image_hash(hdr, image);
    if (ret < 0)
        return ret;
    syslog(LOG_INFO, "Verify image hash successfully\n");

    ret = verify_vela_image_signature(hdr, signature);
    if (ret < 0)
        return ret;
    syslog(LOG_INFO, "Verify image signature successfully\n");

    return 0;
}
