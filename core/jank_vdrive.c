/* SPDX-License-Identifier: GPL-3.0-only */
/* The made-up FAT16 volume and the image stream (jank_vdrive.h). Layout, Microsoft's "FAT: General Overview of On-Disk
 * Format" 1.03: boot sector with the BPB at 0, two FATs, a fixed root directory, then one sector a cluster. */
#include "jank_vdrive.h"
#include "jank_ota.h"
#include <string.h>

#define DIR_ENTRY 32u
#define ATTR_LABEL 0x08u
#define ATTR_ARCHIVE 0x20u
#define ATTR_LFN 0x0Fu
/* 2026-11-15 12:00:00, FAT's packed date and time: every file carries it (there is no clock to give a real one) */
#define FAT_DATE ((uint16_t)(((2026u - 1980u) << 9) | (11u << 5) | 15u))
#define FAT_TIME ((uint16_t)(12u << 11))
#define SEG_HEADER 8u
#define JV_FREE UINT32_MAX              /* a cache entry whose buffer is free */

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, v);
    put16(p + 2, v >> 16);
}

static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static uint32_t clusters_of(uint32_t size) { return (size + JV_SECTOR - 1u) / JV_SECTOR; }

void jv_init(struct jv *d, const uint8_t *html, uint32_t html_size, const char *note, const char *panic, const char *readme, uint32_t serial,
             uint32_t slot_size, jv_alloc_fn alloc, void *alloc_ctx)
{
    uint32_t next = 2;
    memset(d, 0, sizeof *d);
    d->serial = serial;
    d->alloc = alloc;
    d->alloc_ctx = alloc_ctx;
    d->slot_size = slot_size < JV_SLOT_MAX ? slot_size : JV_SLOT_MAX;
    memcpy(d->file[0].sfn, "JANK~1  HTM", 11);
    d->file[0].lfn = "jank.html";
    d->file[0].data = html;
    d->file[0].size = html_size;
    d->nfiles = 1;
    if (note && note[0]) {
        memcpy(d->file[1].sfn, "UPDATE  TXT", 11);
        d->file[1].data = (const uint8_t *)note;
        d->file[1].size = (uint32_t)strlen(note);
        d->nfiles = 2;
    }
    if (panic && panic[0]) {
        struct jv_file *f = &d->file[d->nfiles];
        memcpy(f->sfn, "PANIC   TXT", 11);
        f->data = (const uint8_t *)panic;
        f->size = (uint32_t)strlen(panic);
        d->nfiles++;
    }
    if (readme && readme[0]) {
        struct jv_file *f = &d->file[d->nfiles];
        memcpy(f->sfn, "README  TXT", 11);
        f->lfn = "README.txt";
        f->data = (const uint8_t *)readme;
        f->size = (uint32_t)strlen(readme);
        d->nfiles++;
    }
    for (uint32_t i = 0; i < d->nfiles; i++) {
        d->file[i].cluster = d->file[i].size ? next : 0u;
        next += clusters_of(d->file[i].size);
    }
}

static void boot_sector(const struct jv *d, uint8_t *s)
{
    static const uint8_t jump[3] = {0xEB, 0x3C, 0x90};
    memcpy(s, jump, 3);
    memcpy(s + 3, "MSWIN4.1", 8);
    put16(s + 11, JV_SECTOR);
    s[13] = 1;                         /* sectors a cluster */
    put16(s + 14, JV_FAT_LBA);         /* reserved sectors: the boot sector */
    s[16] = JV_FATS;
    put16(s + 17, JV_ROOT_ENTRIES);
    put16(s + 19, JV_SECTORS);
    s[21] = 0xF8;                      /* fixed disk */
    put16(s + 22, JV_FAT_SECTORS);
    put16(s + 24, 32);                 /* sectors a track, heads: geometry nobody uses */
    put16(s + 26, 2);
    s[36] = 0x80;
    s[38] = 0x29;                      /* the next three fields are there */
    put32(s + 39, d->serial);
    memcpy(s + 43, "JANK       ", 11);
    memcpy(s + 54, "FAT16   ", 8);
    s[510] = 0x55;
    s[511] = 0xAA;
}

/* FAT16 entry n of the made-up volume */
static uint16_t fat_entry(const struct jv *d, uint32_t n)
{
    if (n == 0)
        return 0xFFF8;
    if (n == 1)
        return 0xFFFF;
    for (uint32_t i = 0; i < d->nfiles; i++) {
        const struct jv_file *f = &d->file[i];
        uint32_t c = clusters_of(f->size);
        if (c && n >= f->cluster && n < f->cluster + c)
            return n + 1u == f->cluster + c ? 0xFFFF : (uint16_t)(n + 1u);
    }
    return 0;
}

static uint8_t sfn_sum(const char *sfn)
{
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++)
        sum = (uint8_t)(((sum & 1u) ? 0x80u : 0u) + (sum >> 1) + (uint8_t)sfn[i]);
    return sum;
}

/* one long-name entry holding the whole name (13 characters at most) */
static void lfn_entry(uint8_t *e, const char *name, const char *sfn)
{
    static const uint8_t at[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
    size_t n = strlen(name);
    e[0] = 0x41;                       /* the last (and first) of one */
    e[11] = ATTR_LFN;
    e[13] = sfn_sum(sfn);
    for (size_t i = 0; i < 13; i++) {
        uint16_t c = i < n ? (uint16_t)(uint8_t)name[i] : i == n ? 0u : 0xFFFFu;
        put16(e + at[i], c);
    }
}

static void plain_entry(uint8_t *e, const char *sfn, uint8_t attr, uint32_t cluster, uint32_t size)
{
    memcpy(e, sfn, 11);
    e[11] = attr;
    put16(e + 14, FAT_TIME);
    put16(e + 16, FAT_DATE);
    put16(e + 18, FAT_DATE);
    put16(e + 22, FAT_TIME);
    put16(e + 24, FAT_DATE);
    put16(e + 26, cluster);
    put32(e + 28, size);
}

static void root_first(const struct jv *d, uint8_t *s)
{
    uint8_t *e = s;
    plain_entry(e, "JANK       ", ATTR_LABEL, 0, 0);
    e += DIR_ENTRY;
    for (uint32_t i = 0; i < d->nfiles; i++) {
        const struct jv_file *f = &d->file[i];
        if (f->lfn) {
            lfn_entry(e, f->lfn, f->sfn);
            e += DIR_ENTRY;
        }
        plain_entry(e, f->sfn, ATTR_ARCHIVE, f->cluster, f->size);
        e += DIR_ENTRY;
    }
}

static const uint8_t *cached(const struct jv *d, uint32_t lba)
{
    for (uint32_t i = 0; i < d->ncache; i++)
        if (d->cache_lba[i] == lba)
            return d->cache_buf[i];
    return NULL;
}

static int bit(const uint8_t *map, uint32_t i) { return map[i / 8u] >> (i % 8u) & 1; }
static void set_bit(uint8_t *map, uint32_t i) { map[i / 8u] |= (uint8_t)(1u << (i % 8u)); }

static uint32_t image_sectors(const struct jv *d) { return d->slot_size / JV_SECTOR; }

/* the image's sector for lba, or -1: from its header to the slot's size, or to its length once that is known (what the
 * host writes after the image in its run is another file's) */
static int64_t image_index(const struct jv *d, uint32_t lba)
{
    uint32_t n = image_sectors(d);
    if (d->len.len)
        n = (d->len.len + JV_SECTOR - 1u) / JV_SECTOR;
    if (d->phase == JV_IDLE || d->phase == JV_FAILED || lba < d->start || lba - d->start >= n)
        return -1;
    return (int64_t)(lba - d->start);
}

static void data_sector(const struct jv *d, uint32_t lba, uint8_t *out)
{
    uint32_t cluster = lba - JV_DATA_LBA + 2u;
    for (uint32_t i = 0; i < d->nfiles; i++) {
        const struct jv_file *f = &d->file[i];
        uint32_t c = clusters_of(f->size);
        if (c && cluster >= f->cluster && cluster < f->cluster + c) {
            uint32_t at = (cluster - f->cluster) * JV_SECTOR;
            uint32_t n = f->size - at < JV_SECTOR ? f->size - at : JV_SECTOR;
            memcpy(out, f->data + at, n);
            return;
        }
    }
}

int jv_read(const struct jv *d, uint32_t lba, uint8_t *out, uint32_t *img_offset)
{
    const uint8_t *c;
    int64_t k;
    if (lba >= JV_SECTORS)
        return -1;
    memset(out, 0, JV_SECTOR);
    k = image_index(d, lba);
    if (k >= 0) {
        if (!bit(d->got, (uint32_t)k))
            return 0;
        *img_offset = (uint32_t)k * JV_SECTOR;
        return 1;
    }
    c = cached(d, lba);
    if (c) {
        memcpy(out, c, JV_SECTOR);
        return 0;
    }
    if (lba == 0) {
        boot_sector(d, out);
    } else if (lba < JV_ROOT_LBA) {
        uint32_t first = (lba - JV_FAT_LBA) % JV_FAT_SECTORS * (JV_SECTOR / 2u);
        for (uint32_t i = 0; i < JV_SECTOR / 2u; i++)
            put16(out + 2u * i, fat_entry(d, first + i));
    } else if (lba == JV_ROOT_LBA) {
        root_first(d, out);
    } else if (lba >= JV_DATA_LBA) {
        data_sector(d, lba, out);
    }
    return 0;
}

/* inside one of the made-up files' clusters */
static int in_file(const struct jv *d, uint32_t lba)
{
    uint32_t cluster;
    if (lba < JV_DATA_LBA)
        return 0;
    cluster = lba - JV_DATA_LBA + 2u;
    for (uint32_t i = 0; i < d->nfiles; i++)
        if (d->file[i].size && cluster >= d->file[i].cluster && cluster < d->file[i].cluster + clusters_of(d->file[i].size))
            return 1;
    return 0;
}

static void keep(struct jv *d, uint32_t lba, const uint8_t *data)
{
    uint8_t *buf = (uint8_t *)cached(d, lba);
    for (uint32_t i = 0; !buf && i < d->ncache; i++)
        if (d->cache_lba[i] == JV_FREE) {          /* a buffer whose sector went into the image */
            d->cache_lba[i] = lba;
            buf = d->cache_buf[i];
        }
    if (!buf) {
        if (d->ncache >= JV_CACHE_MAX || !d->alloc || !(buf = d->alloc(d->alloc_ctx))) {
            d->cache_dropped++;
            return;
        }
        d->cache_lba[d->ncache] = lba;
        d->cache_buf[d->ncache++] = buf;
    }
    memcpy(buf, data, JV_SECTOR);
}

/* the op for image sector k: its offset, and the page to erase when nothing of that page was written yet */
static void image_op(struct jv *d, uint32_t k, struct jv_op *op)
{
    uint32_t page;
    op->offset = k * JV_SECTOR;
    page = op->offset / JV_PAGE;
    op->erase_page = -1;
    if (!bit(d->erased, page)) {
        set_bit(d->erased, page);
        op->erase_page = (int32_t)page;
    }
}

static uint32_t early_count(const struct jv *d)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < d->ncache; i++)
        n += d->cache_lba[i] >= JV_DATA_LBA && d->cache_lba[i] < JV_SECTORS;
    return n;
}

static void start_image(struct jv *d, uint32_t lba)
{
    d->phase = JV_STREAM;
    d->start = lba;
    d->frontier = 0;
    d->sectors_in = 0;
    memset(d->got, 0, sizeof d->got);
    memset(d->erased, 0, sizeof d->erased);
    jv_len_init(&d->len);
}

enum jv_kind jv_write(struct jv *d, uint32_t lba, const uint8_t *data, struct jv_op *op)
{
    int64_t k;
    if (lba >= JV_SECTORS || in_file(d, lba))
        return JV_W_IGNORED;
    k = image_index(d, lba);
    if (k < 0 && lba >= JV_DATA_LBA && d->phase != JV_DONE && jo_header_ok(data, JV_SECTOR)) {
        start_image(d, lba);           /* a new image, or the same one copied again: the slot is written afresh */
        image_op(d, 0, op);
        return JV_W_START;
    }
    if (k < 0) {
        if (lba >= JV_DATA_LBA && early_count(d) >= JV_EARLY_MAX)
            d->cache_dropped++;
        else
            keep(d, lba, data);
        return JV_W_KEPT;
    }
    if (d->phase == JV_DONE)           /* the image is whole: what the host writes after it in its run is not the image's */
        return JV_W_IGNORED;
    if (bit(d->got, (uint32_t)k)) {
        op->offset = (uint32_t)k * JV_SECTOR;
        op->erase_page = -1;
        return JV_W_REWRITE;
    }
    image_op(d, (uint32_t)k, op);
    return JV_W_IMAGE;
}

enum jv_kind jv_rewrite(struct jv *d, uint32_t lba, const uint8_t *data, int differs, struct jv_op *op)
{
    if (!differs)
        return JV_W_IGNORED;
    if (image_index(d, lba) == 0 && jo_header_ok(data, JV_SECTOR)) {
        start_image(d, lba);
        image_op(d, 0, op);
        return JV_W_START;
    }
    return JV_W_REWRITE;
}

int jv_take_early(struct jv *d, const uint8_t **data, struct jv_op *op)
{
    for (uint32_t i = 0; i < d->ncache; i++) {
        int64_t k = image_index(d, d->cache_lba[i]);
        if (k <= 0 || bit(d->got, (uint32_t)k))
            continue;
        *data = d->cache_buf[i];
        image_op(d, (uint32_t)k, op);
        d->cache_lba[i] = JV_FREE;     /* it is the image's now: the buffer is free once the firmware has written it */
        return 1;
    }
    return 0;
}

void jv_len_init(struct jv_len *l)
{
    memset(l, 0, sizeof *l);
}

int64_t jv_len_step(struct jv_len *l, uint32_t have, jv_read_fn read, void *ctx)
{
    uint8_t h[JO_HEADER_LEN];
    if (l->len)
        return l->len;
    if (!l->segments) {
        if (have < JO_HEADER_LEN)
            return 0;
        if (read(ctx, 0, h, JO_HEADER_LEN) || !jo_header_ok(h, JO_HEADER_LEN))
            return -1;
        l->segments = h[1];
        l->hash = h[23] == 1u;
        l->next = JO_HEADER_LEN;
    }
    while (l->seen < l->segments && l->next + SEG_HEADER <= have) {
        uint8_t s[SEG_HEADER];
        uint32_t data_len;
        if (read(ctx, l->next, s, SEG_HEADER))
            return -1;
        data_len = get32(s + 4);
        if (data_len > JV_SLOT_MAX)
            return -1;
        l->next += SEG_HEADER + data_len;
        l->seen++;
    }
    if (l->seen < l->segments)
        return 0;
    /* esp_image_format.c, process_checksum: one byte of checksum, the whole padded to 16; then the SHA-256 */
    l->len = ((l->next + 1u + 15u) & ~15u) + (l->hash ? 32u : 0u);
    return l->len;
}

enum jv_phase jv_written(struct jv *d, const struct jv_op *op, jv_read_fn read, void *read_ctx)
{
    uint32_t k = op->offset / JV_SECTOR;
    int64_t len;
    if (d->phase != JV_STREAM)
        return (enum jv_phase)d->phase;
    if (k < image_sectors(d) && !bit(d->got, k)) {
        set_bit(d->got, k);
        d->sectors_in++;
    }
    while (d->frontier < image_sectors(d) && bit(d->got, d->frontier))
        d->frontier++;
    len = jv_len_step(&d->len, d->frontier * JV_SECTOR, read, read_ctx);
    if (len < 0 || len > (int64_t)d->slot_size) {
        d->phase = JV_FAILED;
    } else if (len > 0 && (int64_t)d->frontier * JV_SECTOR >= len) {
        d->phase = JV_DONE;
    }
    return (enum jv_phase)d->phase;
}

void jv_fail(struct jv *d) { d->phase = JV_FAILED; }

uint32_t jv_image_len(const struct jv *d) { return d->len.len; }
