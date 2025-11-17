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

#include <libavb_user/libavb_user.h>
#include <unistd.h>

void usage(const char* progname)
{
    avb_printf("Usage:\n");
    avb_printf("  %s [-i] [-V <vbmeta_partition>] [-k <key_path>] [-s <suffix>] <partition1> [<partition2> ...]\n", progname);
    avb_printf("  %s [-U <image>] [-k <key_path>] [-s <suffix>] <partition1> [<partition2> ...]\n", progname);
    avb_printf("  %s [-I] [-V <vbmeta_partition>] <partition1> [<partition2> ...]\n", progname);
    avb_printf("\n");

    avb_printf("Options:\n");
    avb_printf("  -i                Allow rollback index error\n");
    avb_printf("  -I                Image info mode (display hash descriptor)\n");
    avb_printf("  -k <key_path>     Path to the verification key. Default: '/etc/key.avb'\n");
    avb_printf("  -s <suffix>       Slot suffix, must be '_a' or '_b'. Default: NULL\n");
    avb_printf("  -U <image>        Upgrade verify mode (verify against given image)\n");
    avb_printf("  -V <vbmeta_part>  Use vbmeta partition mode, we must specify a vbmeta partition path\n");
    avb_printf("  -h                Show this help message\n");
    avb_printf("\n");

    avb_printf("Examples:\n");
    avb_printf("  1. Boot Verify\n");
    avb_printf("     %s [-k <key_path>] [-s <suffix>] <partition1> [<partition2> ...]\n", progname);
    avb_printf("\n");
    avb_printf("  2. Boot Verify using vbmeta partition mode\n");
    avb_printf("     %s -V <vbmeta_partition> [-k <key_path>] [-s <suffix>] <partition1> ...\n", progname);
    avb_printf("\n");
    avb_printf("  3. Upgrade Verify\n");
    avb_printf("     %s -U <image> [-k <key_path>] [-s <suffix>] <partition1> [<partition2> ...]\n", progname);
    avb_printf("\n");
    avb_printf("  4. Image Info\n");
    avb_printf("     %s -I <partition1> [<partition2> ...]\n", progname);
    avb_printf("\n");
    avb_printf("  5. Image Info using vbmeta partition mode\n");
    avb_printf("     %s -V <vbmeta_partition> -I <partition1> [<partition2> ...]\n", progname);
}

int main(int argc, char* argv[])
{
    struct avb_params_t params = { 0 };
    struct avb_hash_desc_t hash_desc;
    int partition_count;
    bool info = false;
    int ret;

    params.flags = AVB_SLOT_VERIFY_FLAGS_NO_VBMETA_PARTITION;
    params.key = "/etc/key.avb";

    while ((ret = getopt(argc, argv, "hiIk:s:U:V:")) != -1) {
        switch (ret) {
        case 'i':
            params.flags |= AVB_SLOT_VERIFY_FLAGS_ALLOW_ROLLBACK_INDEX_ERROR;
            break;
        case 'I':
            info = true;
            break;
        case 'k':
            params.key = optarg;
            break;
        case 's':
            params.suffix = optarg;
            break;
        case 'U':
            params.image = optarg;
            break;
        case 'V':
            params.flags &= ~AVB_SLOT_VERIFY_FLAGS_NO_VBMETA_PARTITION;
            params.vbmeta = optarg;
            break;
        case 'h':
        default:
            usage(argv[0]);
            return 1;
            break;
        }
    }

    params.partition = (const char* const*)&argv[optind];
    partition_count = argc - optind;

    if (partition_count < 1) {
        usage(argv[0]);
        return 10;
    }

    if (info) {
        for (int i = 0; i < partition_count; i++) {
            if (params.flags & AVB_SLOT_VERIFY_FLAGS_NO_VBMETA_PARTITION) {
                ret = avb_hash_desc(params.partition[i], &hash_desc);
            } else {
                ret = avb_vbmeta_hash_desc(params.vbmeta, params.partition[i], &hash_desc);
            }

            if (ret != 0) {
                avb_printf("%s get info error for %s %d\n", argv[0], params.partition[i], ret);
                return ret;
            }

            avb_hash_desc_dump(&hash_desc);
        }
        return 0;
    }

    ret = avb_verify(&params);
    if (ret != 0) {
        avb_printf("%s error %d\n", argv[0], ret);
    }

    return ret;
}
