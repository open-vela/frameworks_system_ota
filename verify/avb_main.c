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

#define usage(p) usage_line_num(p, __LINE__)
#define PRINT_VERIFY(o, u, as, ar, p, c) \
    avb_printf("  %-6s | %-6s | %-8s | %-8s | %-6s | %s\n", o, u, as, ar, p, c)

void usage_line_num(const char* progname, const int line_num)
{
    avb_printf("Usage: %s [-b] [-c] [-i] <partition> <key> [suffix]\n", progname);
    avb_printf("       %s [-I] <partition>\n", progname);
    avb_printf("       %s [-U] <image> <partition> <key>\n", progname);

    avb_printf("\nVerify\n");
    PRINT_VERIFY("Option", "Update", "Allow", "Allow", "IDX", "Description");
    PRINT_VERIFY("", "IDX", "Same IDX", "Rollback", "in KV", "");
    PRINT_VERIFY("----", "----", "----", "----", "----", "----");
    PRINT_VERIFY("-b", "Y", "Y", "N", "Y", "Boot verification that enabled by default");
    PRINT_VERIFY("-i", "Y", "Y", "Y", "Y", "Ignore rollback index error during boot verification");
    PRINT_VERIFY("-c", "N", "N", "N", "Y", "Comparing rollback index to prevent duplicate installation");
    PRINT_VERIFY("-U", "N", "Y", "N", "N", "Upgrade verify by comparing the rollback index in partition and image");

    avb_printf("\nInfo print\n");
    avb_printf("  %-4s Show image info\n", "-I");
    avb_printf("  %-4s Show help\n", "-h");

    avb_printf("\nExamples\n");
    avb_printf("  -  Boot Verify\n");
    avb_printf("     %s <partition> <key> [suffix]\n", progname);
    avb_printf("  -  Comparing rollback index\n");
    avb_printf("     %s -c <image> <key> [suffix]\n", progname);
    avb_printf("  -  Upgrade Verify\n");
    avb_printf("     %s -U <image> <partition> <key> [suffix]\n", progname);
    avb_printf("  -  Image Info\n");
    avb_printf("     %s -I <image>\n", progname);

    avb_printf("\nLine num: %d\n", line_num);
}

int main(int argc, char* argv[])
{
    AvbSlotVerifyFlags flags = 0;
    const char* image = NULL;
    int ret;

    while ((ret = getopt(argc, argv, "bchiI:U:")) != -1) {
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
            if (!avb_hash_desc(optarg, &hash_desc)) {
                avb_hash_desc_dump(&hash_desc);
                return 0;
            }
            return 1;
            break;
        case 'U':
            flags |= UTILS_AVB_VERIFY_LOCAL_FLAG_NOKV;
            image = optarg;
            g_rollback_index = 0;
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
    else if (image) {
        ret = avb_verify(image, argv[optind + 1], argv[optind + 2], flags);
        if (ret != 0)
            avb_printf("%s verify %s error %d\n", argv[0], image, ret);
    }

    return ret;
}
