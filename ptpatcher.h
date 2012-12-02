#ifndef __PTPATCHER_H__
#define __PTPATCHER_H__
#define PT_MAGIC 0xFFFFFFFF8F9E8D8BUL
#define PT_VERSION 0x100
#define PT_SIGNATURE_SIZE 0x10
#define PT_RECORD_SIZE 0x1000

/* TODO: Allow these to be overridden getopt():-
 * --pt-offset, -o
 * --pt-size, -s
 * --pt-table-size, -S
 */
#define PT_OFFSET 0x300000
#define PT_SIZE 0x80000
#define PT_TABLE_SIZE 0x500

#define PT_PATCH_OLD "EBT"
#define PT_PATCH_NEW "XBT"

typedef struct
{
    unsigned long long magic;
    unsigned int version;
    unsigned int length;
    unsigned char signature[PT_SIGNATURE_SIZE];
} pt_hdr_t;

typedef struct
{
    unsigned char unused[16];
    unsigned long long magic;
    unsigned int version;
    unsigned int length;
    unsigned int num;
    unsigned char padding[4];
} pt_hdr_inner_t;

typedef struct
{
    unsigned int num;
    char name[4];
    char unused[12];
    char alias[4];
    char padding[56];
} pt_part_t;

typedef int (*pt_iterator_callback_t) (unsigned char *);
#endif
