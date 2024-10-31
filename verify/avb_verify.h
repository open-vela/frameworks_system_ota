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

#ifndef AVB_VERIFY_H
#define AVB_VERIFY_H

#include <libavb.h>

#ifdef __cplusplus
extern "C" {
#endif

struct avb_hash_desc_t {
    uint64_t image_size;
    uint8_t hash_algorithm[32]; /* Ref: struct AvbHashDescriptor */
    uint32_t digest_len;
    uint8_t digest[64]; /* Max: sha512 */
};

int avb_verify(const char* partition, const char* key, const char* suffix, AvbSlotVerifyFlags flags);
int avb_hash_desc(const char* full_partition_name, struct avb_hash_desc_t* desc);
void avb_hash_desc_dump(const struct avb_hash_desc_t* desc);

#ifdef __cplusplus
}
#endif

#endif /* AVB_VERIFY_H */
