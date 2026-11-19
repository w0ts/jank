/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the made-up JANK drive (core/jank_vdrive.h): the volume as a FAT16 reader sees it, and a jank.bin copied on
 * by a modelled host in the orders hosts write in, streamed into a modelled flash slot (erase before write, a byte written
 * once). JANK_TEST_IMAGE=<path to a jank.bin> adds a run with a real image. Exit status: non-zero on a failure. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jank_ota.h"
#include "jank_vdrive.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

#define SLOT (1600u * 1024u)           /* the FH4R2's 0x190000 */
static uint8_t s_slot[JV_SLOT_MAX];
static uint32_t s_slot_size = SLOT;
static int s_flash_faults;             /* a byte written twice without an erase */
static struct jv s_d;
static void *s_bufs[JV_CACHE_MAX];
static int s_nbufs;

static void *alloc_sector(void *ctx)
{
    (void)ctx;
    return s_nbufs < (int)JV_CACHE_MAX ? (s_bufs[s_nbufs++] = malloc(JV_SECTOR)) : NULL;
}

static void free_all(void)
{
    while (s_nbufs)
        free(s_bufs[--s_nbufs]);
}

static int read_slot(void *ctx, uint32_t offset, uint8_t *out, uint32_t n)
{
    (void)ctx;
    if (offset + n > s_slot_size)
        return -1;
    memcpy(out, s_slot + offset, n);
    return 0;
}

/* the firmware's half (firmware/main/drive.c): erase, write, rewrite */
static void flash_write(const struct jv_op *op, const uint8_t *data)
{
    if (op->erase_page >= 0)
        memset(s_slot + (uint32_t)op->erase_page * JV_PAGE, 0xFF, JV_PAGE);
    for (uint32_t i = 0; i < JV_SECTOR; i++) {
        if (s_slot[op->offset + i] != 0xFF)
            s_flash_faults++;
        s_slot[op->offset + i] = data[i];
    }
}

static void flash_rewrite(const struct jv_op *op, const uint8_t *data)
{
    static uint8_t page[JV_PAGE];
    uint32_t base = op->offset / JV_PAGE * JV_PAGE;
    if (!memcmp(s_slot + op->offset, data, JV_SECTOR))
        return;
    memcpy(page, s_slot + base, JV_PAGE);
    memcpy(page + (op->offset - base), data, JV_SECTOR);
    memcpy(s_slot + base, page, JV_PAGE);      /* erase and write back */
}

static enum jv_phase s_phase;
static int s_starts;

static void host_write(uint32_t lba, const uint8_t *data)
{
    struct jv_op op;
    const uint8_t *early;
    enum jv_kind k = jv_write(&s_d, lba, data, &op);
    if (k == JV_W_REWRITE)                         /* as drive.c: compare with the slot first */
        k = jv_rewrite(&s_d, lba, data, memcmp(s_slot + op.offset, data, JV_SECTOR) != 0, &op);
    switch (k) {
    case JV_W_START:
        s_starts++;
        flash_write(&op, data);
        s_phase = jv_written(&s_d, &op, read_slot, NULL);
        while (jv_take_early(&s_d, &early, &op)) {
            flash_write(&op, early);
            s_phase = jv_written(&s_d, &op, read_slot, NULL);
        }
        break;
    case JV_W_IMAGE:
        flash_write(&op, data);
        s_phase = jv_written(&s_d, &op, read_slot, NULL);
        break;
    case JV_W_REWRITE:
        flash_rewrite(&op, data);
        s_phase = jv_written(&s_d, &op, read_slot, NULL);
        break;
    default:
        break;
    }
}

static void host_read(uint32_t lba, uint8_t *out)
{
    uint32_t at = 0;
    int r = jv_read(&s_d, lba, out, &at);
    if (r == 1)
        memcpy(out, s_slot + at, JV_SECTOR);
}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)rd16(p) | (uint32_t)rd16(p + 2) << 16; }
static void wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

static const char HTML[] = "<!doctype html><title>jank</title>... a page long enough to take two sectors ...";
static uint8_t s_html[1300];
static const char *s_panic;                  /* PANIC.TXT's text for the next mount; NULL: none */
static const char *s_readme;                 /* README.txt's text for the next mount; NULL: none */

static void mount(const char *note)
{
    free_all();
    memcpy(s_html, HTML, sizeof HTML - 1u);
    for (size_t i = sizeof HTML - 1u; i < sizeof s_html; i++)
        s_html[i] = (uint8_t)('a' + i % 26u);
    jv_init(&s_d, s_html, sizeof s_html, note, s_panic, s_readme, 0x4A414E4Bu, s_slot_size, alloc_sector, NULL);
    memset(s_slot, 0x5A, sizeof s_slot);           /* the slot's old contents: never 0xFF, so a missing erase shows */
    s_flash_faults = 0;
    s_phase = JV_IDLE;
    s_starts = 0;
}

/* --- the reader's view --- */

static void test_boot_sector(void)
{
    uint8_t s[JV_SECTOR];
    uint32_t clusters;
    mount(NULL);
    host_read(0, s);
    CHECK(s[0] == 0xEB && s[510] == 0x55 && s[511] == 0xAA);
    CHECK(rd16(s + 11) == 512 && s[13] == 1 && rd16(s + 14) == 1 && s[16] == 2 && rd16(s + 17) == 512);
    CHECK(rd16(s + 19) == JV_SECTORS && rd16(s + 22) == JV_FAT_SECTORS && s[21] == 0xF8);
    CHECK(!memcmp(s + 43, "JANK       ", 11) && !memcmp(s + 54, "FAT16   ", 8) && rd32(s + 39) == 0x4A414E4Bu);
    /* Microsoft's rule for the FAT type: by the count of clusters alone */
    clusters = (rd16(s + 19) - rd16(s + 14) - s[16] * rd16(s + 22) - rd16(s + 17) * 32u / 512u) / s[13];
    CHECK(clusters == JV_CLUSTERS && clusters >= 4085u && clusters < 65525u);
    CHECK(rd16(s + 22) * 256u >= clusters + 2u);    /* the FAT covers every cluster */
    CHECK((uint64_t)clusters * 512u >= 2u * 1024u * 1024u + 64u * 1024u);  /* room for the XIAO's 2 MB image and more */
    {
        uint32_t at;
        CHECK(jv_read(&s_d, JV_SECTORS, s, &at) == -1);
    }
}

/* the root's entries: name (long or 8.3), first cluster, size -> how many */
struct entry {
    char name[16];
    uint16_t cluster;
    uint32_t size;
    uint8_t attr;
};

static int list_root(struct entry *out, int max)
{
    uint8_t s[JV_SECTOR];
    char lfn[16] = "";
    int n = 0;
    for (uint32_t lba = JV_ROOT_LBA; lba < JV_DATA_LBA; lba++) {
        host_read(lba, s);
        for (uint32_t i = 0; i < JV_SECTOR; i += 32) {
            const uint8_t *e = s + i;
            if (!e[0])
                return n;
            if (e[0] == 0xE5)
                continue;
            if (e[11] == 0x0F) {
                static const uint8_t at[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
                int k = 0;
                for (int j = 0; j < 13 && e[at[j]] && e[at[j]] != 0xFF; j++)
                    lfn[k++] = (char)e[at[j]];
                lfn[k] = 0;
                continue;
            }
            if (n < max) {
                if (lfn[0])
                    snprintf(out[n].name, sizeof out[n].name, "%s", lfn);
                else
                    snprintf(out[n].name, sizeof out[n].name, "%.11s", (const char *)e);
                out[n].cluster = rd16(e + 26);
                out[n].size = rd32(e + 28);
                out[n].attr = e[11];
                n++;
            }
            lfn[0] = 0;
        }
    }
    return n;
}

static uint16_t fat_get(uint32_t n, int which)
{
    uint8_t s[JV_SECTOR];
    host_read(JV_FAT_LBA + (uint32_t)which * JV_FAT_SECTORS + n / 256u, s);
    return rd16(s + (n % 256u) * 2u);
}

/* a file's bytes through its FAT chain -> 1 when they match want */
static int file_is(uint16_t cluster, uint32_t size, const uint8_t *want)
{
    uint8_t s[JV_SECTOR];
    uint32_t at = 0;
    while (at < size) {
        uint32_t n = size - at < JV_SECTOR ? size - at : JV_SECTOR;
        if (cluster < 2 || cluster >= 0xFFF8)
            return 0;
        host_read(JV_DATA_LBA + cluster - 2u, s);
        if (memcmp(s, want + at, n))
            return 0;
        at += n;
        cluster = fat_get(cluster, 0);
    }
    return cluster >= 0xFFF8;
}

static void test_files(void)
{
    struct entry e[8];
    int n;
    mount(NULL);
    n = list_root(e, 8);
    CHECK(n == 2 && e[0].attr == 0x08 && !strncmp(e[0].name, "JANK", 4));
    CHECK(!strcmp(e[1].name, "jank.html") && e[1].size == sizeof s_html && e[1].cluster == 2);
    CHECK(file_is(e[1].cluster, e[1].size, s_html));
    CHECK(fat_get(0, 0) == 0xFFF8 && fat_get(1, 1) == 0xFFFF && fat_get(2, 1) == 3 && fat_get(4, 0) == 0xFFFF && fat_get(5, 0) == 0);
    mount("jank.bin installed from the drive, restarted\n");
    n = list_root(e, 8);
    CHECK(n == 3 && !strncmp(e[2].name, "UPDATE  TXT", 11) && e[2].cluster == 5);
    CHECK(file_is(e[2].cluster, e[2].size, (const uint8_t *)"jank.bin installed from the drive, restarted\n"));
}

/* README.txt: a file of its own after UPDATE.TXT when there is one, in clusters of its own, readable through its chain */
static void test_readme(void)
{
    static char text[700];
    struct entry e[8];
    int n;
    for (size_t i = 0; i < sizeof text - 1u; i++)
        text[i] = i % 60u == 59u ? '\n' : (char)('A' + i % 26u);
    text[sizeof text - 1u] = 0;
    s_readme = text;
    mount(NULL);
    n = list_root(e, 8);
    CHECK(n == 3 && !strcmp(e[2].name, "README.txt") && e[2].size == strlen(text) && e[2].cluster == 5);
    CHECK(file_is(e[2].cluster, e[2].size, (const uint8_t *)text));
    CHECK(fat_get(5, 0) == 6 && fat_get(6, 0) == 0xFFFF && fat_get(7, 0) == 0 && fat_get(5, 1) == 6);
    mount("jank.bin installed from the drive, restarted\n");
    n = list_root(e, 8);
    CHECK(n == 4 && !strncmp(e[2].name, "UPDATE  TXT", 11) && e[2].cluster == 5);
    CHECK(!strcmp(e[3].name, "README.txt") && e[3].cluster == 6 && e[3].size == strlen(text));
    CHECK(file_is(e[3].cluster, e[3].size, (const uint8_t *)text));
    CHECK(file_is(e[2].cluster, e[2].size, (const uint8_t *)"jank.bin installed from the drive, restarted\n"));
    s_readme = "";
    mount(NULL);
    CHECK(list_root(e, 8) == 2);                    /* empty text: no file */
    s_readme = NULL;
}

/* PANIC.TXT: after UPDATE.TXT and before README.txt, only when there is a text, readable through its chain */
static void test_panic_file(void)
{
    static char text[700];
    struct entry e[8];
    int n;
    for (size_t i = 0; i < sizeof text - 1u; i++)
        text[i] = i % 50u == 49u ? '\n' : (char)('a' + i % 26u);
    text[sizeof text - 1u] = 0;
    s_panic = text;
    mount(NULL);
    n = list_root(e, 8);
    CHECK(n == 3 && !strncmp(e[2].name, "PANIC   TXT", 11) && e[2].size == strlen(text) && e[2].cluster == 5);
    CHECK(file_is(e[2].cluster, e[2].size, (const uint8_t *)text));
    CHECK(fat_get(5, 0) == 6 && fat_get(6, 0) == 0xFFFF && fat_get(7, 0) == 0);
    s_readme = "the readme\n";
    mount("the note\n");
    n = list_root(e, 8);
    CHECK(n == 5 && !strncmp(e[2].name, "UPDATE  TXT", 11) && !strncmp(e[3].name, "PANIC   TXT", 11) && !strcmp(e[4].name, "README.txt"));
    CHECK(e[2].cluster == 5 && e[3].cluster == 6 && e[4].cluster == 8);
    CHECK(file_is(e[2].cluster, e[2].size, (const uint8_t *)"the note\n") && file_is(e[3].cluster, e[3].size, (const uint8_t *)text) &&
          file_is(e[4].cluster, e[4].size, (const uint8_t *)"the readme\n"));
    s_panic = "";
    s_readme = NULL;
    mount(NULL);
    CHECK(list_root(e, 8) == 2);                    /* empty text: no file */
    s_panic = NULL;
}

/* --- images --- */

/* an ESP32-S3 app image of segs segments of the given sizes: header, segments, checksum padding, SHA-256 -> its length */
static uint32_t make_image(uint8_t *img, const uint32_t *segs, unsigned nseg, int hash)
{
    uint32_t at = JO_HEADER_LEN, len;
    memset(img, 0, JO_HEADER_LEN);
    img[0] = 0xE9;
    img[1] = (uint8_t)nseg;
    img[12] = JO_CHIP_ESP32S3;
    img[23] = (uint8_t)(hash ? 1 : 0);
    for (unsigned i = 0; i < nseg; i++) {
        uint8_t *h = img + at;
        h[0] = 0x00, h[1] = 0x00, h[2] = 0x3C, h[3] = 0x42;
        h[4] = (uint8_t)segs[i], h[5] = (uint8_t)(segs[i] >> 8), h[6] = (uint8_t)(segs[i] >> 16), h[7] = (uint8_t)(segs[i] >> 24);
        at += 8u;
        for (uint32_t k = 0; k < segs[i]; k++)
            img[at + k] = (uint8_t)(k * 7u + i * 13u + 1u);  /* never a valid header at a sector start: 0xE9 is rare */
        at += segs[i];
    }
    len = ((at + 1u + 15u) & ~15u) + (hash ? 32u : 0u);
    for (uint32_t k = at; k < len; k++)
        img[k] = (uint8_t)(0xA0u + k);
    return len;
}

static uint8_t s_img[JV_SLOT_MAX + 4096];

/* the host's FAT16 writer: the first free run of clusters for size bytes -> its first cluster */
static uint16_t free_run(uint32_t size)
{
    uint32_t need = (size + JV_SECTOR - 1u) / JV_SECTOR, run = 0;
    for (uint32_t c = 2; c < JV_CLUSTERS + 2u; c++) {
        run = fat_get(c, 0) ? 0u : run + 1u;
        if (run == need)
            return (uint16_t)(c - need + 1u);
    }
    return 0;
}

static void fat_chain(uint16_t first, uint32_t size)
{
    uint8_t s[JV_SECTOR];
    uint32_t n = (size + JV_SECTOR - 1u) / JV_SECTOR;
    for (int which = 0; which < 2; which++)
        for (uint32_t lba = 0; lba < JV_FAT_SECTORS; lba++) {
            int touched = 0;
            host_read(JV_FAT_LBA + (uint32_t)which * JV_FAT_SECTORS + lba, s);
            for (uint32_t i = 0; i < 256u; i++) {
                uint32_t c = lba * 256u + i;
                if (c >= first && c < first + n) {
                    wr16(s + i * 2u, c + 1u == first + n ? 0xFFFFu : c + 1u);
                    touched = 1;
                }
            }
            if (touched)
                host_write(JV_FAT_LBA + (uint32_t)which * JV_FAT_SECTORS + lba, s);
        }
}

static void dir_add(const char *sfn, uint16_t cluster, uint32_t size)
{
    uint8_t s[JV_SECTOR];
    host_read(JV_ROOT_LBA, s);
    for (uint32_t i = 0; i < JV_SECTOR; i += 32)
        if (!s[i]) {
            memset(s + i, 0, 32);
            memcpy(s + i, sfn, 11);
            s[i + 11] = 0x20;
            wr16(s + i + 26, cluster);
            s[i + 28] = (uint8_t)size, s[i + 29] = (uint8_t)(size >> 8), s[i + 30] = (uint8_t)(size >> 16), s[i + 31] = (uint8_t)(size >> 24);
            break;
        }
    host_write(JV_ROOT_LBA, s);
}

enum order { IN_ORDER, CHUNKS_BACKWARDS, HEADER_LAST_OF_FIRST_CHUNK, TWICE_SAME, TWICE_DIFFERENT };

/* a host copies data of size bytes as a new file: data sectors in the order given, then the FAT, then the directory */
static uint16_t copy_file(const char *sfn, const uint8_t *data, uint32_t size, enum order order)
{
    uint16_t first = free_run(size);
    uint32_t n = (size + JV_SECTOR - 1u) / JV_SECTOR;
    static uint8_t sec[JV_SECTOR];
#define PUT(k)                                                                         \
    do {                                                                               \
        uint32_t k_ = (k), m_ = size - k_ * JV_SECTOR;                                 \
        memset(sec, 0, sizeof sec);                                                    \
        memcpy(sec, data + k_ * JV_SECTOR, m_ < JV_SECTOR ? m_ : JV_SECTOR);           \
        host_write(JV_DATA_LBA + first - 2u + k_, sec);                                \
    } while (0)
    if (!first)
        return 0;
    if (order == IN_ORDER || order == TWICE_SAME || order == TWICE_DIFFERENT) {
        for (uint32_t k = 0; k < n; k++) {
            if (order == TWICE_DIFFERENT && k == n / 2u) {   /* other bytes first, then the file's: a rewrite */
                memset(sec, 0x33, sizeof sec);
                host_write(JV_DATA_LBA + first - 2u + k, sec);
            }
            PUT(k);
            if (order == TWICE_SAME && k == n / 2u)
                PUT(k);
        }
    } else if (order == CHUNKS_BACKWARDS) {         /* the header first, then 64 KB commands from the end back */
        PUT(0);
        for (uint32_t c = (n - 1u) / 128u + 1u; c-- > 0;)
            for (uint32_t k = c * 128u; k < n && k < c * 128u + 128u; k++)
                if (k)
                    PUT(k);
    } else {                                        /* the first 16 sectors' data before the header */
        for (uint32_t k = 1; k < 16u && k < n; k++)
            PUT(k);
        PUT(0);
        for (uint32_t k = 16; k < n; k++)
            PUT(k);
    }
#undef PUT
    fat_chain(first, size);
    dir_add(sfn, first, size);
    return first;
}

static void test_stream(enum order order, int hash, int apple_double)
{
    static uint8_t junk[4096];
    static const uint32_t segs[] = {12345, 70001, 4096, 300000, 9};
    uint32_t len = make_image(s_img, segs, 5, hash);
    struct entry e[8];
    uint16_t first;
    int n;
    mount(NULL);
    if (apple_double) {                             /* macOS: the ._ file before the data */
        memset(junk, 0, sizeof junk);
        junk[1] = 0x05, junk[2] = 0x16, junk[3] = 0x07;
        copy_file("_JANK   BIN", junk, sizeof junk, IN_ORDER);
    }
    first = copy_file("JANK    BIN", s_img, len, order);
    CHECK(first != 0);
    CHECK(s_phase == JV_DONE && jv_image_len(&s_d) == len && s_starts == 1);
    CHECK(!memcmp(s_slot, s_img, len) && s_flash_faults == 0);
    /* the host reads its file and its directory back as it wrote them */
    n = list_root(e, 8);
    CHECK(n == (apple_double ? 4 : 3) && !strncmp(e[n - 1].name, "JANK    BIN", 11) && e[n - 1].size == len);
    CHECK(file_is(e[n - 1].cluster, e[n - 1].size, s_img));
    CHECK(file_is(e[1].cluster, e[1].size, s_html));
    if (apple_double)
        CHECK(file_is(e[2].cluster, e[2].size, junk));
    /* after the image: the host's next file in the same run is not the image's (and reads back from the cache) */
    {
        uint8_t s[JV_SECTOR], t[JV_SECTOR];
        uint32_t after = JV_DATA_LBA + first - 2u + (len + JV_SECTOR - 1u) / JV_SECTOR;
        memset(s, 0x77, sizeof s);
        host_write(after, s);
        host_read(after, t);
        CHECK(!memcmp(s, t, sizeof s) && !memcmp(s_slot, s_img, len));
    }
}

static void test_orders(void)
{
    test_stream(IN_ORDER, 1, 0);
    test_stream(IN_ORDER, 0, 0);
    test_stream(IN_ORDER, 1, 1);
    test_stream(CHUNKS_BACKWARDS, 1, 1);
    test_stream(HEADER_LAST_OF_FIRST_CHUNK, 1, 0);
    test_stream(TWICE_SAME, 1, 0);
    test_stream(TWICE_DIFFERENT, 1, 0);
}

static void test_not_images(void)
{
    static uint8_t data[20000];
    static const uint32_t segs[] = {1000};
    uint32_t len;
    /* plain files: kept, never streamed */
    mount(NULL);
    memset(data, 0xE9, sizeof data);                 /* starts like an image but is not an S3's */
    copy_file("NOTES   TXT", data, sizeof data, IN_ORDER);
    CHECK(s_starts == 0 && s_phase == JV_IDLE);
    /* an image bigger than the slot: refused once its length is known */
    {
        static const uint32_t big[] = {SLOT};
        mount(NULL);
        len = make_image(s_img, big, 1, 1);
        copy_file("BIG     BIN", s_img, 8192, IN_ORDER);   /* its first sectors are enough to know */
        CHECK(s_starts == 1 && s_phase == JV_FAILED);
    }
    /* a segment count the loader refuses: no start */
    mount(NULL);
    len = make_image(s_img, segs, 1, 1);
    s_img[1] = 17;
    copy_file("BAD     BIN", s_img, len, IN_ORDER);
    CHECK(s_starts == 0);
    /* cut short (the host stopped): never done */
    mount(NULL);
    len = make_image(s_img, segs, 1, 1);
    copy_file("CUT     BIN", s_img, 512, IN_ORDER);
    CHECK(s_starts == 1 && s_phase == JV_STREAM);
    /* a failed image, then a good copy: the second one starts afresh */
    {
        static const uint32_t big[] = {SLOT};
        static const uint32_t good[] = {5000, 600};
        mount(NULL);
        make_image(s_img, big, 1, 1);
        copy_file("BIG     BIN", s_img, 4096, IN_ORDER);
        CHECK(s_phase == JV_FAILED);
        len = make_image(s_img, good, 2, 1);
        copy_file("JANK    BIN", s_img, len, IN_ORDER);
        CHECK(s_starts == 2 && s_phase == JV_DONE && !memcmp(s_slot, s_img, len) && s_flash_faults == 0);
    }
    /* a copy cancelled half way (no FAT, no directory written: its clusters stay free), then another image copied into
     * the same clusters: its header comes as a rewrite of the first's first sector and starts the stream afresh */
    {
        static const uint32_t a[] = {300000, 2000}, b[] = {12345, 99999, 7};
        static uint8_t first[JV_SLOT_MAX];
        uint32_t la, lb;
        mount(NULL);
        la = make_image(first, a, 2, 1);
        {
            uint16_t c = free_run(la);
            static uint8_t sec[JV_SECTOR];
            for (uint32_t k = 0; k < la / JV_SECTOR / 2u; k++) {
                memcpy(sec, first + k * JV_SECTOR, JV_SECTOR);
                host_write(JV_DATA_LBA + c - 2u + k, sec);
            }
            CHECK(s_phase == JV_STREAM);
        }
        lb = make_image(s_img, b, 3, 1);
        copy_file("JANK    BIN", s_img, lb, IN_ORDER);
        CHECK(s_starts == 2 && s_phase == JV_DONE && jv_image_len(&s_d) == lb && !memcmp(s_slot, s_img, lb) && s_flash_faults == 0);
    }
    /* writes to the made-up files are dropped: jank.html reads the same */
    {
        uint8_t s[JV_SECTOR], t[JV_SECTOR];
        struct jv_op op;
        mount(NULL);
        memset(s, 0x11, sizeof s);
        CHECK(jv_write(&s_d, JV_DATA_LBA, s, &op) == JV_W_IGNORED);
        host_read(JV_DATA_LBA, t);
        CHECK(!memcmp(t, s_html, JV_SECTOR));
        CHECK(jv_write(&s_d, JV_SECTORS, s, &op) == JV_W_IGNORED);
    }
}

static void test_cache_limits(void)
{
    uint8_t s[JV_SECTOR];
    uint32_t dropped;
    mount(NULL);
    memset(s, 0x42, sizeof s);
    for (uint32_t k = 0; k < JV_EARLY_MAX + 10u; k++)
        host_write(JV_DATA_LBA + 100u + k, s);
    CHECK(s_d.cache_dropped == 10u);                 /* data sectors: JV_EARLY_MAX at most */
    dropped = s_d.cache_dropped;
    for (uint32_t lba = 1; lba < JV_DATA_LBA; lba++)  /* metadata: always room */
        host_write(lba, s);
    CHECK(s_d.cache_dropped == dropped);
    {
        uint8_t t[JV_SECTOR];
        host_read(JV_ROOT_LBA + 3u, t);
        CHECK(!memcmp(s, t, sizeof s));
    }
}

/* the length parser on its own, against the real file when one is given */
static void test_len(void)
{
    static const uint32_t segs[] = {100, 200};
    struct jv_len l;
    uint32_t len = make_image(s_img, segs, 2, 1);
    memcpy(s_slot, s_img, len);
    s_slot_size = SLOT;
    jv_len_init(&l);
    CHECK(jv_len_step(&l, 10, read_slot, NULL) == 0);       /* the header is not in yet */
    CHECK(jv_len_step(&l, 24 + 8, read_slot, NULL) == 0);   /* one segment header of two */
    CHECK(jv_len_step(&l, 24 + 8 + 100 + 8, read_slot, NULL) == (int64_t)len);
    CHECK(len == ((24u + 8u + 100u + 8u + 200u + 1u + 15u) & ~15u) + 32u);
    {
        const char *path = getenv("JANK_TEST_IMAGE");
        FILE *f = path ? fopen(path, "rb") : NULL;
        if (f) {
            size_t got = fread(s_slot, 1, JV_SLOT_MAX, f);
            fclose(f);
            s_slot_size = JV_SLOT_MAX;
            jv_len_init(&l);
            CHECK(jv_len_step(&l, (uint32_t)got, read_slot, NULL) == (int64_t)got);
            printf("vdrive: %s: %zu bytes, length from its headers agrees\n", path, got);
            s_slot_size = SLOT;
        }
    }
}

int main(void)
{
    test_boot_sector();
    test_files();
    test_readme();
    test_panic_file();
    test_len();
    test_orders();
    test_not_images();
    test_cache_limits();
    free_all();
    printf("vdrive: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
