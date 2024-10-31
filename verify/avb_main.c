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

#include "avb_verify.h"
#include <unistd.h>

void usage(const char* progname)
{
    avb_printf("Usage: %s [-b] [-c] [-i] <partition> <key> [suffix]\n", progname);
    avb_printf("       %s [-I] <partition>\n", progname);
    avb_printf("Examples:\n");
    avb_printf("  1. Boot Verify\n");
    avb_printf("     %s <partition> <key> [suffix]\n", progname);
    avb_printf("  2. Upgrade Verify\n");
    avb_printf("     %s -c <image> <key> [suffix]\n", progname);
    avb_printf("  3. Image Info\n");
    avb_printf("     %s -I <image>\n", progname);
}

int main(int argc, char* argv[])
{
    AvbSlotVerifyFlags flags = 0;
    int ret;

    while ((ret = getopt(argc, argv, "bchiI")) != -1) {
        switch (ret) {
        case 'b':
            break;
        case 'c':
            flags |= AVB_SLOT_VERIFY_FLAGS_NOT_ALLOW_SAME_ROLLBACK_INDEX | AVB_SLOT_VERIFY_FLAGS_NOT_UPDATE_ROLLBACK_INDEX;
            break;
        case 'i':
            flags |= AVB_SLOT_VERIFY_FLAGS_ALLOW_ROLLBACK_INDEX_ERROR;
            break;
        case 'h':
            usage(argv[0]);
            return 0;
            break;
        case 'I':
            struct avb_hash_desc_t hash_desc;
            if (argc - optind < 2) {
                usage(argv[0]);
                return 100;
            }
            if (!avb_hash_desc(argv[optind + 1], &hash_desc)) {
                avb_hash_desc_dump(&hash_desc);
                return 0;
            }
            return 1;
            break;
        default:
            usage(argv[0]);
            return 10;
            break;
        }
    }

    if (argc - optind < 2) {
        usage(argv[0]);
        return 100;
    }

    ret = avb_verify(argv[optind], argv[optind + 1], argv[optind + 2], flags);
    if (ret != 0)
        avb_printf("%s error %d\n", argv[0], ret);

    return ret;
}
