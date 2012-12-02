/* ptpatcher.c
 * patchin' PTs since 2012.
 *
 * Copyright (C) 2012 AndroidRoot.mobi
 *
 * !! UNRELEASED INTERNAL TOOL !!
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ptpatcher.h"

#ifdef __arm__
#define DEV_MMCBLK_DEVICE "/dev/block/mmcblk0"
#define DEV_MMCBLK_BOOT0 "/dev/block/mmcblk0boot0"
#define DEV_MMCBLK_BOOT1 "/dev/block/mmcblk0boot1"
#define DEV_MMCBLK_BOOT0_PROTECT "/sys/block/mmcblk0boot0/force_ro"
#define DEV_MMCBLK_BOOT1_PROTECT "/sys/block/mmcblk0boot1/force_ro"
#include "nvaes.h"
#else
/* TODO: Allow these to be overriden by getopt() with, say, --pt, -T */
#define DEV_MMCBLK_DEVICE "blk0.img"
#define DEV_MMCBLK_BOOT0 "boot0.img"
#define DEV_MMCBLK_BOOT1 "boot1.img"
#include "aes-cmac.h"
#endif

static int FOUND_PT_OFFSET = 0;

static int sign_pt(unsigned char *pt, unsigned char *signature)
{
    pt_hdr_t *pt_hdr = (pt_hdr_t *)pt;

    #ifdef __arm__
    char key[AES_BLOCK_SIZE] = {0};
    nvaes_ctx ctx;

    if(!(ctx = nvaes_open())) {
        perror("failed to open AES engine");
        return 0;
    }

    nvaes_set_key(ctx, key);

    nvaes_sign(ctx, &pt[sizeof(pt_hdr_t)], pt_hdr->length-sizeof(pt_hdr_t),
               signature);

    nvaes_close(ctx);
    #else
    cmac_hash(&pt[sizeof(pt_hdr_t)],pt_hdr->length-sizeof(pt_hdr_t),signature);
    #endif

    return 1;
}

static int read_pt(unsigned char *pt)
{
    int offset = PT_OFFSET, i = 0;
    unsigned int remaining = PT_SIZE, bytes = 0;
    FILE *fp;

    for(i=0; i<3; i++) {
        char filename[128] = {0};
        switch(i) {
            case 0: strcpy(filename, DEV_MMCBLK_BOOT0); break;
            case 1: strcpy(filename, DEV_MMCBLK_BOOT1); break;
            case 2: strcpy(filename, DEV_MMCBLK_DEVICE); break;
        }

        if(!(fp = fopen(filename, "rb"))) {
            perror("failed to open file");
            return 1;
        }

        if(i < 2) {
            fseek(fp, 0, SEEK_END);

            offset -= ftell(fp);

            if(offset >= 0) {
                fclose(fp);
                continue;
            }

            fseek(fp, offset, SEEK_CUR);
        }

        bytes = fread(pt + (PT_SIZE - remaining), sizeof(unsigned char),
                      remaining, fp);
        fclose(fp);

        remaining -= bytes;
        if(remaining == 0) break;
    }

    /*
    fp = fopen("pt", "wb");
    fwrite(pt, sizeof(unsigned char), PT_SIZE, fp);
    fclose(fp);
    */

    return remaining == 0;
}

static int write_pt(unsigned char *pt)
{
    int offset = PT_OFFSET, i = 0;
    unsigned int remaining = PT_SIZE, bytes = 0;
    FILE *fp;

    for(i=0; i<3; i++) {
        char filename[128] = {0};
        switch(i) {
            case 0: strcpy(filename, DEV_MMCBLK_BOOT0); break;
            case 1: strcpy(filename, DEV_MMCBLK_BOOT1); break;
            case 2: strcpy(filename, DEV_MMCBLK_DEVICE); break;
        }

        if(!(fp = fopen(filename, "wb+"))) {
            perror("failed to open file");
            return 0;
        }

        if(i < 2) {
            fseek(fp, 0, SEEK_END);

            offset -= ftell(fp);

            if(offset >= 0) {
                fclose(fp);
                continue;
            }

            fseek(fp, offset, SEEK_CUR);
        }

        bytes = fwrite(pt + (PT_SIZE - remaining), sizeof(unsigned char),
                      remaining, fp);
        fclose(fp);

        remaining -= bytes;
        if(remaining == 0) break;
    }

    return remaining == 0;
}


static int iterate_pt(unsigned char *pt, pt_iterator_callback_t callback)
{
    unsigned int num_pts = PT_SIZE / PT_RECORD_SIZE, i = 0;

    for(i = 0; i < num_pts; i++) {
        if(!callback(&pt[i * PT_RECORD_SIZE])) {
            return 0;
        }
    }

    return 1;
}

static int verify_pt(unsigned char *pt)
{
    unsigned char signature[PT_SIGNATURE_SIZE];
    pt_hdr_t *pt_hdr = (pt_hdr_t *)pt;
    pt_hdr_inner_t *pt_hdr_inner = (pt_hdr_inner_t *)&pt[sizeof(pt_hdr_t)];

    if(pt_hdr->magic != PT_MAGIC || pt_hdr->version != PT_VERSION) {
        fprintf(stderr, "invalid PT image");
        return 0;
    }

    if(pt_hdr_inner->magic != PT_MAGIC || pt_hdr_inner->version != PT_VERSION) {
        fprintf(stderr, "invalid PT image (inner)");
        return 0;
    }

    sign_pt(pt, signature);

    if(memcmp(pt_hdr->signature, signature, PT_SIGNATURE_SIZE)) {
        fprintf(stderr, "invalid PT signature");
        return 0;
    }

    return 1;
}

static int patch_pt(unsigned char *pt)
{
    unsigned char *ptr = pt;
    int i = 0;

    pt_hdr_t *pt_hdr = (pt_hdr_t *)ptr;
    pt_hdr_inner_t *pt_hdr_inner = (pt_hdr_inner_t *)&ptr[sizeof(pt_hdr_t)];
    pt_part_t *pt_parts = (pt_part_t *)&ptr[sizeof(pt_hdr_t) +
                                            sizeof(pt_hdr_inner_t)];

    for(i = 0; i < pt_hdr_inner->num; i++) {
        if(!strcmp((&pt_parts[i])->name, PT_PATCH_OLD)) {
            if(!strcmp((&pt_parts[i])->alias, PT_PATCH_OLD)) {
                strcpy((&pt_parts[i])->name, PT_PATCH_NEW);
                strcpy((&pt_parts[i])->alias, PT_PATCH_NEW);
            }
        }
    }

    return sign_pt(pt, pt_hdr->signature);
}

#ifdef __arm__
static int toggle_pt_protection(char mode)
{
    FILE *fp;
    char cmd[3] = {mode, '\n', 0};

    if(mode != '0' && mode != '1') {
        fprintf(stderr, "invalid mode: %c\n", mode);
        return 0;
    }

    fp = fopen(DEV_MMCBLK_BOOT0_PROTECT, "w");
    if(fwrite(cmd, sizeof(char), sizeof(cmd), fp) != sizeof(cmd)) {
        return 0;
    }
    fclose(fp);

    fp = fopen(DEV_MMCBLK_BOOT1_PROTECT, "w");
    if(fwrite(cmd, sizeof(char), sizeof(cmd), fp) != sizeof(cmd)) {
        return 0;
    }
    fclose(fp);

    return 1;
}

static int restore_pt(unsigned char *pt)
{
    /* Helper for main() since it's called from two locations. */
    printf("attempting to restore original pt...");
    if(!write_pt(pt)) {
        printf("failed!\n\n");
        printf("IMPORTANT!! YOUR DEVICE COULD BE IN A BRICKING STATE\n");
        printf("POWER THE DEVICE FROM A MAINS ADAPTER\n");
        printf("AND SEEK HELP FROM IRC IMMEDIATELY!\n\n");
        (void)toggle_pt_protection('1');
        return 1;
    }
    printf("done\n");
    (void)toggle_pt_protection('1');
    return 0;
}
#endif

int main(int argc, const char **argv)
{
    unsigned char *pt = NULL, *pt_orig = NULL;
    int consumed = 0;
    FILE *fp;

    setvbuf(stdout,NULL,_IONBF,0);

    printf("ptpatcher\n");
    printf("---------\n\n");

    if(!(pt = (unsigned char *)malloc(PT_SIZE))) {
        perror("failed to allocate pt memory");
        return 1;
    }

    if(!(pt_orig = (unsigned char *)malloc(PT_SIZE))) {
        perror("failed to allocate pt memory");
        return 1;
    }

    printf("reading pt...");
    if(!read_pt(pt)) {
        printf("failed!\n");
        return 1;
    }
    memcpy(pt_orig, pt, PT_SIZE);
    printf("done\n");

    printf("verifying pt...");
    if(!iterate_pt(pt, &verify_pt)) {
        printf("failed!\n");
        return 1;
    }
    printf("done\n");

    printf("patching pt...");
    if(!iterate_pt(pt, &patch_pt)) {
        printf("failed!\n");
        return 1;
    }
    printf("done\n");

    printf("verifying patched pt...");
    if(!iterate_pt(pt, &verify_pt)) {
        printf("failed!\n");
        return 1;
    }
    printf("done\n");

    #ifdef __arm__
    fp = NULL; // Prevent variable unused warnings.
    printf("writing pt...");

    if(!toggle_pt_protection('0')) {
        printf("failed.\n");
        return 1;
    }

    if(!write_pt(pt)) {
        printf("failed!\n");
        return restore_pt(pt_orig);
    }
    printf("done\n");

    printf("reading updated pt...");
    memset(pt, 0, PT_SIZE);
    if(!read_pt(pt)) {
        printf("failed!\n");
        return restore_pt(pt_orig);
    }
    printf("done\n");

    printf("verifying updated pt...");
    if(!iterate_pt(pt, &verify_pt)) {
        printf("failed!\n");
        return restore_pt(pt_orig);
    }
    printf("done\n");
    (void)toggle_pt_protection('1');
    #else
    if(argc > 1) {
        if(!(fp = fopen(argv[1], "wb"))) {
            perror("failed to open output file");
            return 1;
        }

        fwrite(pt, sizeof(unsigned char), PT_SIZE, fp);
        fclose(fp);

        printf("wrote patched pt to: %s\n", argv[1]);
    } else {
        fprintf(stderr, "no argument given - not outputting patched pt!\n");
        return 1;
    }
    #endif

    return 0;
}
