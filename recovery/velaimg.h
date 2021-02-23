#ifndef _VELA_IMAGE_H_
#define _VELA_IMAGE_H_

#define VELA_MAGIC "VELAOS!!"
#define VELA_MAGIC_SIZE 8

struct vela_img_hdr {
    uint8_t magic[VELA_MAGIC_SIZE];

    uint32_t image_size; /* image size in bytes */
    uint32_t sign_size; /* signature size in bytes */

    uint8_t hash[32]; /* sha256 hash */
};

typedef struct vela_img_hdr vela_img_hdr;

int verify_vela_image(vela_img_hdr* hdr, char* image, char* signature);

#endif
