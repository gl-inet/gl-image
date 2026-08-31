/*
 * This file is part of sp-oops-extract
 *
 * MTD Oops/Panic log extraction program
 *
 * Copyright (C) 2007, 2008 Nokia Corporation. All rights reserved.
 *
 * Author: Richard Purdie <rpurdie@openedhand.com>
 * Contact: Eero Tamminen <eero.tamminen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#define _XOPEN_SOURCE 500

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <err.h>
#include <stdint.h>
#include <guci.h>
#include <stdlib.h>

#define OOPS_PAGE_SIZE 4096

#define MEMGETINFO                _IOR('M', 1, struct mtd_info_user)

struct mtd_info_user {
    uint8_t type;
    uint32_t flags;
    uint32_t size;   // Total size of the MTD
    uint32_t erasesize;
    uint32_t writesize;
    uint32_t oobsize;   // Amount of OOB data per block (e.g. 16)
    /* The below two fields are obsolete and broken, do not use them
     * (TODO: remove at some point) */
    uint32_t ecctype;
    uint32_t eccsize;
};

static int try_to_check_for_bad_blocks(void)
{
    /* FIXME
     * Checking for bad blocks should be implemented here.
     */
    return 0;
}

int main(const int argc, const char *argv[])
{
    uint32_t *count, maxcount = 0xffffffff;
    uint32_t *magic_ptr, magic_value = 0x5d005d00;

    unsigned char *charbuf;
    unsigned long size;
    const char *device;
    struct stat sbuf;
    struct mtd_info_user meminfo;
    int i, j, maxpos;
    void *buf;
    int fd;
    char record_size_buf[16] = {0};
    int record_size = 0;

    fprintf(stderr, "Oops Log extractor (build %s %s).\n", __DATE__, __TIME__);
    if (argc < 2) {
        errx(-1, "Usage: %s devicefile\n", argv[0]);
    }

    device = argv[1];

    struct uci_context *ctx = guci_init();
    guci_get(ctx, "gl_logread.crash.record_size", record_size_buf, sizeof(record_size_buf));
    guci_free(ctx);
    record_size = atoi(record_size_buf);
    if (record_size < 1) {
        record_size = OOPS_PAGE_SIZE;
    }

    buf = malloc(record_size);
    if (!buf) {
        err(-1, "Unable to allocate memory");
    }

    if (stat(device, &sbuf)) {
        err(-1, "Unable to stat file '%s'", device);
    }

    fd = open(device, O_RDONLY);
    if (fd < 0) {
        err(-1, "Unable to open file '%s'", device);
    }


    if (S_ISBLK(sbuf.st_mode)) {
        if (ioctl(fd, BLKGETSIZE, &size) < 0) {
            err(-1, "BLKGETSIZE ioctl failed for device '%s'", device);
        }
        size = size * 512;
        if (try_to_check_for_bad_blocks())
            warnx("%s is probably an mtdblock device. Thus it's not possible to check for bad blocks", device);
    } else if (S_ISCHR(sbuf.st_mode)) {
        if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
            warn("MEMGETINFO ioctl failed for device '%s'", device);
            errx(-1, "%s is maybe not an mtd device?", device);
        }
        size = meminfo.size;
        if (try_to_check_for_bad_blocks()) {
            warnx("Checking for bad blocks failed");
            warnx("Are you sure that %s is an mtd device?", device);
        }

    } else if (S_ISREG(sbuf.st_mode)) {
        size = sbuf.st_size;
    } else if (S_ISLNK(sbuf.st_mode)) {
        errx(-1, "%s is a symbolik link", device);
    } else if (S_ISDIR(sbuf.st_mode)) {
        errx(-1, "%s is a directory", device);
    } else if (S_ISFIFO(sbuf.st_mode)) {
        errx(-1, "%s is a FIFO", device);
    } else if (S_ISSOCK(sbuf.st_mode)) {
        errx(-1, "%s is a socket", device);
    } else
        errx(-1, "%s is something weird", device);

    charbuf = buf;
    count = (uint32_t *) buf;
    magic_ptr = (uint32_t *)(buf + sizeof(uint32_t));

    for (i = 0; i < (size / record_size); i++) {
        pread(fd, buf, record_size, i * record_size);
        if (*count == 0xffffffff || *magic_ptr != magic_value)
            continue;
        if (maxcount == 0xffffffff) {
            maxcount = *count;
            maxpos = i;
        } else if ((*count < 0x40000000) && (maxcount > 0xc0000000)) {
            maxcount = *count;
            maxpos = i;
        } else if ((*count > maxcount) && (*count < 0xc0000000)) {
            maxcount = *count;
            maxpos = i;
        } else if ((*count > maxcount) && (*count > 0xc0000000)
                   && (maxcount > 0x80000000)) {
            maxcount = *count;
            maxpos = i;
        }
    }

    if (maxcount == 0xffffffff) {
        fprintf(stderr, "No logs present\n");
        return 0;
    }
    fprintf(stderr, "Last log is at position %d with count %u\n", maxpos, maxcount);

    for (i = 0; i < (size / record_size); i++) {
        int bufend;

        maxpos++;
        if ((maxpos * record_size) >= size)
            maxpos = 0;

        pread(fd, buf, record_size, maxpos * record_size);
        if (*count == 0xffffffff || *magic_ptr != magic_value)
            continue;

        fprintf(stdout, "Log Entry %u (at position %d)\n", *count, maxpos);
        for (j = record_size - 1; j > 3; j--) {
            if (charbuf[j] != 0xff)
                break;
        }
        bufend = j;
        for (j = 8; j <= bufend; j++)
            fprintf(stdout, "%c", charbuf[j]);
        fprintf(stdout, "\n");
    }
    return 0;
}
