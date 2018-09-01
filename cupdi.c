/*
Copyright(c) 2016 Atmel Corporation, a wholly owned subsidiary of Microchip Technology Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

"""
pyupdi is a Python utility for programming AVR devices with UPDI interface
using a standard TTL serial port.

Connect RX and TX together with a suitable resistor and connect this node
to the UPDI pin of the AVR device.

Be sure to connect a common ground, and use a TTL serial adapter running at
the same voltage as the AVR device.

Vcc                     Vcc
+ -++-+
|                       |
+-------------------- - +|                       | +-------------------- +
| Serial port + -++-+AVR device        |
|                     |      +---------- + |                    |
|                  TX + ------ + 4k7 + -------- - +UPDI               |
|                     |      +---------- + |    |                    |
|                     |                      |    |                    |
|                  RX + ---------------------- + |                    |
|                     |                           |                    |
|                     +-- + +-- + |
+-------------------- - +|                     |  +-------------------- +
+-++-+
GND                   GND


This is C version of UPDI interface achievement, referred to the Python version here:
    https://github.com/mraardvark/pyupdi.git
*/

#include <stdio.h>
#include <stdbool.h>
#include <os/platform.h>
#include <argparse/argparse.h>
#include <device/device.h>
#include <updi/nvm.h>
#include <ihex/ihex.h>
#include <string/split.h>
#include "cupdi.h"

static const char *const usage[] = {
    "Simple command line interface for UPDI programming:",
    "updi [options] [[--] args]",
    "Erase chip: updi -c COM2 -d tiny817 -e ",
    "Flash hex file: updi -c COM2 -d tiny817 -f c:/817.hex",
    NULL,
};

int main(int argc, char *argv[])
{
    char *dev_name = NULL;
    char *comport = NULL;
    char *file = NULL;
    char *fuses = NULL;
    bool do_erase, skip_erase = true;
    bool debug, skip_debug = false;
    int baudrate = 115200;

    const device_info_t * dev;
    void *nvm_ptr;
    int result;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("Basic options"),
        OPT_STRING('d', "device", &dev_name, "Target device"),
        OPT_STRING('c', "comport", &comport, "Com port to use (Windows: COMx | *nix: /dev/ttyX)"),
        OPT_BOOLEAN('e', "erase", &skip_erase, "Perform a chip erase (implied with --flash)"),
        OPT_INTEGER('b', "baudrate", &baudrate, "Baud rate, default=115200"),
        OPT_STRING('f', "flash", &file, "Intel HEX file to flash"),
        OPT_STRING('u', "fuses", &fuses, "Fuse to set (syntax: fuse_nr:0xvalue)"),
        OPT_BOOLEAN('v', "verbose", &skip_debug, "Set verbose mode"),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usage, 0);
    argparse_describe(&argparse, "\nA brief description of what the program does and how it works.", "\nAdditional description of the program after the description of the arguments.");
    
    if (argc <= 1) {
        argparse_usage(&argparse);
        return 0;
    }

    argc = argparse_parse(&argparse, argc, argv);
    if (argc != 0) {
        printf("argc: %d\n", argc);
        int i;
        for (i = 0; i < argc; i++) {
            printf("argv[%d]: %s\n", i, *(argv + i));
        }
    }

    do_erase = !skip_erase;
    if (!(fuses || file || do_erase )) {
        printf("No action (erase, flash or fuses)\r\n");
        return 0;
    }

    if (!dev_name) {
        printf("No DEV Name appointed\r\n");
        return 0;
    }

    if (!comport) {
        printf("No COM PORT appointed\r\n");
        return 0;
    }

    debug = !skip_debug;
    if (debug) {
        ; //TODO
    }

    dev = get_chip_info(dev_name);
    if (!dev) {
        printf("Device %s not support\r\n", dev_name);
        return -2;
    }

    nvm_ptr = updi_nvm_init(comport, baudrate, (void *)dev);
    if (!nvm_ptr) {
        printf("Nvm initialize failed\r\n");
        result = -3;
        goto out;
    }

    result = nvm_get_device_info(nvm_ptr);
    if (result) {
        printf("nvm_get_device_info failed\r\n");
        result = -4;
        goto out;
    }

    result = nvm_enter_progmode(nvm_ptr);
    if (result) {
        printf("Device is locked(%d). Performing unlock with chip erase.\r\n", result);
        result = nvm_unlock_device(nvm_ptr);
        if (result) {
            printf("NVM unlock device failed %d\r\n", result);
            return -5;
        }
    }

    //programming
    if (do_erase) {
        result = updi_erase(nvm_ptr);
        if (result) {
            printf("NVM chip erase failed %d\r\n", result);
            result = -6;
            goto out;
        }
    }

    if (fuses) {
        result = updi_fuse(nvm_ptr, fuses);
        if (result) {
            printf("NVM set fuse failed %d\r\n", result);
            result = -7;
            goto out;
        }
    }

    if (file) {
        result = updi_flash(nvm_ptr, file);
        if (result) {
            printf("NVM update failed %d\r\n", result);
            result = -8;
            goto out;
        }
    }

out:
    nvm_leave_progmode(nvm_ptr);
    updi_nvm_deinit(nvm_ptr);

    return result;
}

int updi_erase(void *nvm_ptr)
{
    int result;

    result = nvm_chip_erase(nvm_ptr);
    if (result) {
        printf("NVM chip erase failed %d\r\n", result);
        return -2;
    }

    return 0;
}

int updi_fuse(void *nvm_ptr, char *fuses)
{
    char** tokens;
    int idx, value;
    int i, result;

    tokens = str_split(fuses, ':');
    if (tokens)
    {
        for (i = 0; tokens[i]; i++) {
            printf("Fuse[%d]: %s\r\n", i, tokens[i]);

            if (i == 0)
                idx = (int)strtol(tokens[i], NULL, 10);
            else if (i == 1)
                value = (int)strtol(tokens[i], NULL, 16);
            else
                break;
            free(tokens[i]);
        }
        free(tokens);

        if (i == 2) {
            result = nvm_write_fuse(nvm_ptr, idx, (u8)value);
            if (result) {
                printf("nvm_write_fuse failed %d\r\n", result);
                return -2;
            }
        }
        else {
            printf("parse token count num = %d failed\r\n", i);
            return -3;
        }
        return 0;
    }

    printf("parse str: %s failed\r\n", fuses);
    return -4;
}

hex_info_t *load_hex(const char *file)
{
    hex_info_t info, *ihex = NULL;
    int result;

    memset(&info, 0, sizeof(info));
    result = get_hex_info(file, &info);
    if (result) {
        printf("get_hex_info failed %d\r\n", result);
        return NULL;
    }
    
    ihex = (hex_info_t *)malloc(info.total_size + sizeof(*ihex));
    if (!ihex) {
        printf("malloc hexi memory(%d) failed", ihex->total_size);
        return NULL;
    }
    memcpy(ihex, &info, sizeof(*ihex));
    ihex->data = (unsigned char *)(ihex + 1);
    memset(ihex->data, 0xff, ihex->total_size);
    ihex->len = ihex->total_size;

    result = get_hex_info(file, ihex);
    if (result) {
        printf("get_hex_info failed %d\r\n", result);
        result = -4;
        goto failed;
    }

    return ihex;

failed:
    unload_hex(ihex);
    return NULL;
}

void unload_hex(hex_info_t *ihex)
{
    if (ihex) {
        free(ihex);
    }
}

int adjust_hex(hex_info_t **ihex_ptr, flash_info_t *info)
{
    hex_info_t *ihex, *ihex_new;
    int from, to, size, len;
    int mask = info->flash_pagesize - 1;
    
    ihex = *ihex_ptr;
    if ((ihex->addr_from & mask) || (ihex->total_size & mask)) {
        from = ihex->addr_from & ~mask;
        to = ((ihex->addr_to + mask) & ~mask) - 1;
        size = to - from + 1;
        
        len = size + sizeof(*ihex_new);
        ihex_new = (hex_info_t *)malloc(len);
        if (!ihex_new) {
            printf("adjust hex malloc %d failed\r\n", len);
            return -2;
        }

        ihex_new->data = (unsigned char *)(ihex_new + 1);
        memset(ihex_new->data, 0xff, size);
        memcpy(ihex_new->data + (ihex->addr_from & mask), ihex->data, ihex->total_size);
        ihex_new->addr_from = from;
        ihex_new->addr_to = to;
        ihex_new->actual_size = ihex->actual_size;
        ihex_new->total_size = size;
        ihex_new->len = size;
        
        unload_hex(ihex);
        *ihex_ptr = ihex = ihex_new;
    }
   
    // Offset to actual flash start
    if (ihex->addr_from < info->flash_start) {
        ihex->addr_from += info->flash_start;
        ihex->addr_to += info->flash_start;
    }

    // Size check
    if (ihex->addr_to >= (u32)(info->flash_start + info->flash_size)) {
        printf("hex addr(%04x ~ %04x) over flash size \r\n", ihex->addr_from, ihex->addr_to);
        return -3;
    }

    return 0;
}

int updi_verify_flash(void *nvm_ptr, hex_info_t *ihex)
{
    u8 * rdata;
    int i, result;

    //compare data
    rdata = malloc(ihex->len);
    if (!rdata) {
        printf("malloc rdata failed\r\n");
        return -2;
    }

    result = nvm_read_flash(nvm_ptr, ihex->addr_from, rdata, ihex->len);
    if (result) {
        printf("nvm_read_flash failed %d\r\n", result);
        result = -3;
        goto out;
    }

    for (i = 0; i < ihex->len; i++) {
        if (ihex->data[i] != rdata[i]) {
            printf("check flash data failed at %d, %02x-%02x\r\n", i, ihex->data[i], rdata[i]);
            break;
        }
    }

    if (i < ihex->len) {
        printf("data verifcation failed\r\n");
        result = -4;
        goto out;
    }

out:
    if (rdata)
        free(rdata);

    return result;
}

int updi_flash(void *nvm_ptr, const char *file)
{
    hex_info_t *ihex = NULL;
    flash_info_t flash;
    u8 *rdata = NULL;
    int result = 0;

    result = nvm_get_flash_info(nvm_ptr, &flash);
    if (result) {
        printf("nvm_get_flash_info failed %d\r\n", result);
        result = -4;
        goto out;
    }

    ihex = load_hex(file);
    if (!ihex) {
        printf("updi_load_hex failed\r\n");
        return -2;
    }

    // Offset to actual flash start
    result = adjust_hex(&ihex, &flash);
    if (result) {
        printf("adjust_hex failed %d\r\n", result);
        result = -5;
        goto out;
    }
    
    result = nvm_chip_erase(nvm_ptr);
    if (result) {
        printf("nvm_chip_erase failed %d\r\n", result);
        result = -3;
        goto out;
    }

    result = nvm_write_flash(nvm_ptr, ihex->addr_from, ihex->data, ihex->len);
    if (result) {
        printf("nvm_write_flash failed %d\r\n", result);
        result = -3;
        goto out;
    }
    
    result = updi_verify_flash(nvm_ptr, ihex);
    if (result) {
        printf("nvm_write_flash failed %d\r\n", result);
        result = -3;
        goto out;
    }
out:
    unload_hex(ihex);
    return result;
}