/* SPDX-License-Identifier: GPL-3.0-only */
/* The firmware's update flow on the host, for tests/test_ota.py: lines on stdin, the device's answers on stdout, the image
 * as "flashed" in the file named by argv[1] when "ota end" succeeds. Same calls as firmware/main/ota.c, with memory for flash. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jank_ota.h"

#define SLOT (1u << 20)

int main(int argc, char **argv)
{
    static uint8_t flash[SLOT];
    struct jo j;
    struct jo_cmd c;
    char line[JO_LINE_MAX], out[64];
    uint32_t now = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    jo_init(&j);
    if (argc < 2)
        return 2;
    while (fgets(line, sizeof line, stdin)) {
        enum jo_err e = JO_E_ARG;
        uint32_t at = j.offset;
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "idle ", 5)) {            /* the test moves the clock */
            now += (uint32_t)strtoul(line + 5, NULL, 10);
            e = jo_idle(&j, now);
            printf("%s", e == JO_OK ? "tick\n" : "dropped\n");
            continue;
        }
        switch (jo_parse(line, &c)) {
        case JO_BEGIN:
            e = jo_begin(&j, c.size, c.crc, SLOT, now);
            if (e == JO_OK) {
                printf("ok 0 %u\n", JO_CHUNK_MAX);
                continue;
            }
            break;
        case JO_DATA:
            at = c.offset;
            e = jo_data(&j, &c, now);
            if (e == JO_OK)
                memcpy(flash + c.offset, c.data, c.n);
            if (e == JO_OK || e == JO_OK_DUP || e == JO_E_GAP || e == JO_E_CRC || e == JO_E_HEADER)
                at = j.offset;
            break;
        case JO_END: {
            uint32_t size = j.size;
            e = jo_end(&j);
            if (e == JO_OK) {
                FILE *f = fopen(argv[1], "wb");
                if (!f || fwrite(flash, 1, size, f) != size)
                    e = JO_E_ARG;
                if (f)
                    fclose(f);
            }
            at = j.offset;
            break;
        }
        case JO_ABORT:
            jo_abort(&j);
            e = JO_OK;
            at = 0;
            break;
        default:
            break;
        }
        jo_reply(e, at, out, sizeof out);
        fputs(out, stdout);
    }
    return 0;
}
