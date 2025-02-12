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
    avb_printf("Usage: %s [-b] [-i] <partition> <key> [suffix]\n", progname);
    avb_printf("       %s [-U] <image> <partition> <key>\n", progname);
    avb_printf("       %s [-I] <partition>\n", progname);

    avb_printf("\nExamples\n");
    avb_printf("  -  Boot Verify\n");
    avb_printf("     %s <partition> <key> [suffix]\n", progname);
    avb_printf("  -  Upgrade Verify\n");
    avb_printf("     %s -U <image> <partition> <key> [suffix]\n", progname);
    avb_printf("  -  Image Info\n");
    avb_printf("     %s -I <image>\n", progname);
}

int main(int argc, char* argv[])
{
    struct avb_params_t params = { 0 };
    int ret;

    while ((ret = getopt(argc, argv, "bhiI:U:")) != -1) {
        switch (ret) {
        case 'b':
            break;
        case 'i':
            params.flags |= AVB_SLOT_VERIFY_FLAGS_ALLOW_ROLLBACK_INDEX_ERROR;
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
            params.image = optarg;
            break;
        case 'h':
            usage(argv[0]);
            return 0;
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

    params.partition = argv[optind];
    params.key = argv[optind + 1];
    params.suffix = argv[optind + 2];

    ret = avb_verify(&params);
    if (ret != 0)
        avb_printf("%s error %d\n", argv[0], ret);

    return ret;
}
