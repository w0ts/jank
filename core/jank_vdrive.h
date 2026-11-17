/* SPDX-License-Identifier: GPL-3.0-only */
/* The JANK drive as a made-up FAT16 volume (the UF2 bootloaders' trick, for a plain jank.bin): nothing of it is stored.
 * Reads are worked out from a table: the boot sector, the FATs, the root directory with jank.html, UPDATE.TXT and
 * README.txt, and their data from memory. A jank.bin copied onto it is recognised by its first sector (an ESP32-S3 app image header) and
 * goes STRAIGHT into the spare app slot as it arrives: no FAT on flash, no copy of the file anywhere, so it works on a
 * 4 MB chip whose two app slots leave no room for a drive that could hold the file (the FH4R2).
 *
 * The file's place: hosts give a new file free clusters in order, and this volume's free space is one run, so the image's
 * byte k is in the sector (start + k / 512), start being the sector its header came in. Sectors may come in any order after
 * that; a sector of the run that came BEFORE the header (a host that writes out of order) is kept in the cache and moved
 * into the slot when the header comes. The image's length comes from its own header and segment headers (the ESP-IDF
 * image format: 24 byte header, segments of 8 byte header + data, a checksum byte padding the whole to 16, and a SHA-256
 * when hash_appended); once every sector up to that length is in, the image is done and the firmware checks it
 * (esp_ota_set_boot_partition verifies the segments and the SHA-256) and restarts.
 *
 * What else the host writes (FAT, directories, the macOS ._ files, .fseventsd) is kept in a small cache, so that it reads
 * back what it wrote while the volume is mounted; past the cache it reads zeros. Nothing survives a restart: the drive
 * comes back with jank.html (and UPDATE.TXT, the last update's note) only. */
#ifndef JANK_VDRIVE_H
#define JANK_VDRIVE_H
#include <stddef.h>
#include <stdint.h>

#define JV_SECTOR 512u
#define JV_SECTORS 8192u               /* 4 MB: room for a 2 MB image (the XIAO's slot) and what the host adds */
#define JV_FATS 2u
#define JV_FAT_SECTORS 32u             /* 8192 FAT16 entries >= the clusters + 2 */
#define JV_ROOT_ENTRIES 512u
#define JV_ROOT_SECTORS (JV_ROOT_ENTRIES * 32u / JV_SECTOR)
#define JV_FAT_LBA 1u
#define JV_ROOT_LBA (JV_FAT_LBA + JV_FATS * JV_FAT_SECTORS)
#define JV_DATA_LBA (JV_ROOT_LBA + JV_ROOT_SECTORS)
#define JV_CLUSTERS (JV_SECTORS - JV_DATA_LBA) /* one sector a cluster: 8095, FAT16 (4085 .. 65524) */
#define JV_SLOT_MAX (2u * 1024u * 1024u) /* the largest app slot the bitmaps cover */
#define JV_PAGE 4096u                  /* the flash's erase unit */
#define JV_CACHE_MAX 160u              /* sectors the host wrote that are kept (allocated as they come) */
#define JV_EARLY_MAX 64u               /* of those, data sectors that came before an image's header and may be its own */

/* one file of the made-up volume: an 8.3 name ("JANK~1  HTM", 11 characters, space padded), an optional long name */
struct jv_file {
    char sfn[11];
    const char *lfn;                   /* NULL: none; at most 13 characters */
    const uint8_t *data;
    uint32_t size;
    uint32_t cluster;                  /* its first; the rest follow (set by jv_init) */
};

enum jv_phase { JV_IDLE = 0, JV_STREAM, JV_DONE, JV_FAILED };

/* the image's length, from its headers: the parser asks for bytes once they are in */
struct jv_len {
    uint32_t next;                     /* where the next header starts (offset in the image) */
    uint8_t segments, seen, hash;      /* segment count, segment headers read, hash_appended */
    uint32_t len;                      /* the whole image, 0 until known */
};

typedef void *(*jv_alloc_fn)(void *ctx);   /* one JV_SECTOR buffer, or NULL */
typedef int (*jv_read_fn)(void *ctx, uint32_t offset, uint8_t *out, uint32_t n);   /* image bytes already in the slot: 0 ok */

struct jv {
    struct jv_file file[3];
    uint32_t nfiles;
    uint32_t serial;
    /* the cache: what the host wrote outside the image */
    uint32_t cache_lba[JV_CACHE_MAX];
    uint8_t *cache_buf[JV_CACHE_MAX];
    uint32_t ncache, cache_dropped;
    jv_alloc_fn alloc;
    void *alloc_ctx;
    /* the image */
    uint8_t phase;                     /* enum jv_phase */
    uint32_t slot_size;                /* bytes; JV_SLOT_MAX at most */
    uint32_t start;                    /* its first sector's LBA */
    uint32_t frontier;                 /* sectors 0 .. frontier-1 of the image are all in */
    uint32_t sectors_in;
    uint8_t got[JV_SLOT_MAX / JV_SECTOR / 8u];     /* by image sector */
    uint8_t erased[JV_SLOT_MAX / JV_PAGE / 8u];    /* by flash page of the slot */
    struct jv_len len;
};

/* html: the control page; note: UPDATE.TXT's text (NULL or empty: no such file); readme: README.txt's text, listed last
 * (NULL or empty: no such file). serial: the volume's id */
void jv_init(struct jv *d, const uint8_t *html, uint32_t html_size, const char *note, const char *readme, uint32_t serial,
             uint32_t slot_size, jv_alloc_fn alloc, void *alloc_ctx);

/* a read: the sector at lba into out. -> -1 out of range; 0 done; 1 it is the image's: out is to be read from the slot
 * at *img_offset (a sector of the image not in yet reads as zeros: out is zeroed and 0 returned) */
int jv_read(const struct jv *d, uint32_t lba, uint8_t *out, uint32_t *img_offset);

/* what a written sector is */
enum jv_kind {
    JV_W_IGNORED = 0,                  /* out of range, or written to the made-up files: dropped */
    JV_W_KEPT,                         /* in the cache (or dropped when it is full: cache_dropped counts) */
    JV_W_IMAGE,                        /* the image's: write it at op->offset (erase op->erase_page first when set) */
    JV_W_START,                        /* an image's header: a new image starts here (a running one is dropped); then as JV_W_IMAGE */
    JV_W_REWRITE                       /* a sector of the image already in, written again: if the slot holds other bytes,
                                          the firmware reads its page, puts the sector in, erases and writes the page back */
};
struct jv_op {
    uint32_t offset;                   /* in the slot */
    int32_t erase_page;                /* -1: none; else the page (offset / JV_PAGE) to erase before writing */
};
/* the host wrote one sector: data is JV_SECTOR bytes */
enum jv_kind jv_write(struct jv *d, uint32_t lba, const uint8_t *data, struct jv_op *op);
/* after JV_W_REWRITE, once the firmware has compared the sector with the slot (differs: 1 when the slot holds other
 * bytes): the image's first sector written again with another image header is a new copy over a half-finished one in
 * the same clusters (a cancelled copy leaves the run free): the stream starts afresh -> JV_W_START (op set as for it).
 * Otherwise -> JV_W_REWRITE, or JV_W_IGNORED when nothing differs. */
enum jv_kind jv_rewrite(struct jv *d, uint32_t lba, const uint8_t *data, int differs, struct jv_op *op);
/* after JV_W_START: the cached sectors that belong to the new image, one at a time -> 1 and data/op set; 0 none left.
 * Write each as JV_W_IMAGE. */
int jv_take_early(struct jv *d, const uint8_t **data, struct jv_op *op);
/* the firmware wrote op's sector: mark it in, move the frontier, read the length's headers when they are in. -> phase
 * (JV_DONE: every byte of the image is in the slot) */
enum jv_phase jv_written(struct jv *d, const struct jv_op *op, jv_read_fn read, void *read_ctx);
/* the firmware gave the image up (flash error, not its turn): it is dropped */
void jv_fail(struct jv *d);
/* the whole length once known, else 0 */
uint32_t jv_image_len(const struct jv *d);

/* the length parser on its own (tests): feed it the image's first n bytes */
void jv_len_init(struct jv_len *l);
/* reads the headers that lie wholly below have (bytes in) -> the length, 0 not known yet, -1 not an image */
int64_t jv_len_step(struct jv_len *l, uint32_t have, jv_read_fn read, void *ctx);

#endif
