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

    My purpose is you could use UPDI at any enviroment like Computer / MCU Embedded system, just transplant the code the there.
    I used other vendor library, thanks:
        argparse: https://github.com/cofyc/argparse.git
        ihex:       https://github.com/arkku/ihex.git
        sercom:     https://github.com/dpogue/Terminal-Emulator.git
    If you have any question, please query Pater to mailbox atpboy444@hotmail.com
    If you like it please help to mark STAR at https://github.com/PitterL/cupdi.git
 */

#include <stdio.h>
#include <os/platform.h>
#include <argparse/argparse.h>
#include <device/device.h>
#include <updi/nvm.h>
#include <ihex/ihex.h>
#include <string/split.h>
#include <time.h>
#include "cupdi.h"

static const char *const usage[] = {
    "Simple command line interface for UPDI programming:",
    "cupdi [options] [[--] args]",
    "Erase chip: cupdi -c COM2 -d tiny817 -e ",
    "Flash hex file: cupdi -c COM2 -d tiny817 -f c:/817.hex",
    NULL,
};

enum { FLAG_UNLOCK, FLAG_ERASE, FLAG_PROG, FLAG_CHECK, FLAG_SAVE};
enum { OP_READ, OP_WRITE };

int main(int argc, char *argv[])
{
    char *dev_name = NULL;
    char *comport = NULL;
    int baudrate = 115200;
    char *file = NULL;
    char *fuses = NULL;
    char *read = NULL;
    char *write = NULL;
    char *dbgview = NULL;
    int flag = 0;
    bool unlock = false;
    int verbose = 1;
    bool test = false;

    const device_info_t * dev;
    void *nvm_ptr;
    int result;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("Basic options"),
        OPT_STRING('d', "device", &dev_name, "Target device"),
        OPT_STRING('c', "comport", &comport, "Com port to use (Windows: COMx | *nix: /dev/ttyX)"),
        OPT_INTEGER('b', "baudrate", &baudrate, "Baud rate, default=115200"),
        OPT_STRING('f', "file", &file, "Intel HEX file to flash"),
        OPT_BIT('u', "unlock", &flag, "Perform a chip unlock (implied with --unlock)", NULL, (1 << FLAG_UNLOCK), 0),
        OPT_BIT('e', "erase", &flag, "Perform a chip erase (implied with --flash)", NULL, (1 << FLAG_ERASE), 0),
        OPT_BIT('p', "program", &flag, "Program Intel HEX file to flash", NULL, (1 << FLAG_PROG), 0),
        OPT_BIT('k', "check", &flag, "Compare Intel HEX file with flash content", NULL, (1 << FLAG_CHECK), 0),
        OPT_BIT('s', "save", &flag, "Save flash to a Intel HEX file", NULL, (1 << FLAG_SAVE), 0),
        OPT_STRING('u', "fuses", &fuses, "Fuse to set (syntax: fuse_nr:0xvalue)"),
        OPT_STRING('r', "read", &read, "Direct read from memory [addr];[n]"),
        OPT_STRING('w', "write", &write, "Direct write to memory [addr];[dat0];[dat1];[dat2]..."),
        OPT_STRING('g', "dbgview", &dbgview, "get ref/delta/cc value operation ds=[ptc_qtlib_node_stat1]|dr=[qtlib_key_data_set1]|loop=[n]|keys=[n] (loop(Hex) set to 0 loop forvever, default 1, keys default 1)"),
        OPT_INTEGER('v', "verbose", &verbose, "Set verbose mode (SILENCE|UPDI|NVM|APP|LINK|PHY|SER): [0~6], default 0, suggest 2 for status information"),
        OPT_BOOLEAN('t', "test", &test, "Test UPDI device"),
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
        DBG_INFO(DEFAULT_DEBUG, "argc: %d\n", argc);
        for (int i = 0; i < argc; i++) {
            DBG_INFO(DEFAULT_DEBUG, "argv[%d]: %s\n", i, argv[i]);
        }
    }

    //set parameter
    set_verbose_level(verbose);

    if (!dev_name) {
        DBG_INFO(UPDI_DEBUG, "No DEV Name appointed");
        return ERROR_PTR;
    }

    if (!comport) {
        DBG_INFO(UPDI_DEBUG, "No COM PORT appointed");
        return ERROR_PTR;
    }

    if (file) {
        if (!flag)
            SET_BIT(flag, FLAG_PROG);
    }

    if (TEST_BIT(flag, FLAG_UNLOCK) || TEST_BIT(flag, FLAG_ERASE) || TEST_BIT(flag, FLAG_PROG))
        unlock = true;

    dev = get_chip_info(dev_name);
    if (!dev) {
        DBG_INFO(UPDI_DEBUG, "Device %s not support", dev_name);
        return -2;
    }

    nvm_ptr = updi_nvm_init(comport, baudrate, (void *)dev);
    if (!nvm_ptr) {
        DBG_INFO(UPDI_DEBUG, "Nvm initialize failed");
        result = -3;
        goto out;
    }

    result = nvm_get_device_info(nvm_ptr);
    if (result) {
        DBG_INFO(UPDI_DEBUG, "nvm_get_device_info failed");
        result = -4;
        goto out;
    }

    //programming
    if (unlock) {
        result = nvm_enter_progmode(nvm_ptr);
        if (result) {
            DBG_INFO(UPDI_DEBUG, "Device is locked(%d). Performing unlock with chip erase.", result);
            result = nvm_unlock_device(nvm_ptr);
            if (result) {
                DBG_INFO(UPDI_DEBUG, "NVM unlock device failed %d", result);
                result = -5;
                goto out;
            }
        }

        result = nvm_get_device_info(nvm_ptr);
        if (result) {
            DBG_INFO(UPDI_DEBUG, "nvm_get_device_info in program failed");
            result = -6;
            goto out;
        }
    }

    if (TEST_BIT(flag, FLAG_ERASE)) {
        result = updi_erase(nvm_ptr);
        if (result) {
            DBG_INFO(UPDI_DEBUG, "NVM chip erase failed %d", result);
            result = -7;
            goto out;
        }
    }

    if (fuses) {
        result = updi_fuse(nvm_ptr, fuses);
        if (result) {
            DBG_INFO(UPDI_DEBUG, "NVM set fuse failed %d", result);
            result = -8;
            goto out;
        }
    }

    if (file) {
        if (TEST_BIT(flag, FLAG_PROG) || TEST_BIT(flag, FLAG_CHECK)) {
            result = updi_flash(nvm_ptr, file, TEST_BIT(flag, FLAG_PROG));
            if (result) {
                DBG_INFO(UPDI_DEBUG, "updi_flash failed %d", result);
                result = -9;
                goto out;
            }
        }

        if (TEST_BIT(flag, FLAG_SAVE)) {
            result = updi_save(nvm_ptr, file);
            if (result) {
                DBG_INFO(UPDI_DEBUG, "NVM save failed %d", result);
                result = -10;
                goto out;
            }
        }
    }

    if (read) {
        result = updi_read(nvm_ptr, read);
        if (result) {
            DBG_INFO(UPDI_DEBUG, "Read failed %d", result);
            result = -11;
            goto out;
        }
    }

    if (write) {
        result = updi_write(nvm_ptr, write);
        if (result) {
            DBG_INFO(UPDI_DEBUG, "Write failed %d", result);
            result = -12;
            goto out;
        }
    }

    if (dbgview) {
        result = updi_debugview(nvm_ptr, dbgview);
        if (result) {
            DBG_INFO(UPDI_DEBUG, "Debugview failed %d", result);
            result = -12;
            goto out;
        }
    }

out:
    nvm_leave_progmode(nvm_ptr);
    updi_nvm_deinit(nvm_ptr);

    return result;
}

/*
    Erase the chip
    @nvm_ptr: updi_nvm_init() device handle
    @returns 0 - success, other value failed code
*/
int updi_erase(void *nvm_ptr)
{
    int result;

    result = nvm_chip_erase(nvm_ptr);
    if (result) {
        DBG_INFO(UPDI_DEBUG, "NVM chip erase failed %d", result);
        return -2;
    }

    return 0;
}

/*
    Write fuse
    @nvm_ptr: updi_nvm_init() device handle
    @fuses: fuse message string. format: [Fuse item]:[Value]
    @returns 0 - success, other value failed code
*/
int updi_fuse(void *nvm_ptr, char *fuses)
{
    char** tokens;
    int idx, value;
    int i, result;

    tokens = str_split(fuses, ':');
    if (!tokens) {
        DBG_INFO(UPDI_DEBUG, "Parse fuse str: %s failed", fuses);
        return -2;
    }

    for (i = 0; tokens[i]; i++) {
        DBG_INFO(UPDI_DEBUG, "Fuse[%d]: %s", i, tokens[i]);

        if (i == 0)
            idx = (int)strtol(tokens[i], NULL, 10);
        else if (i == 1)
            value = (int)strtol(tokens[i], NULL, 16);

        free(tokens[i]);
    }
    free(tokens);

    if (i == 2) {
        result = nvm_write_fuse(nvm_ptr, idx, (u8)value);
        if (result) {
            DBG_INFO(UPDI_DEBUG, "nvm_write_fuse failed %d", result);
            return -3;
        }
    }
    else {
        DBG_INFO(UPDI_DEBUG, "Parse token count num = %d failed", i);
        return -4;
    }
    
    DBG_INFO(UPDI_DEBUG, "Write Fuse[%d]: %02x", idx, value);

    return 0;
}

/*
    load hex file to memory, this will align the address and size to flash page size, patch with 0xff
    @file: Hex file path
    @iflash: chip flash information, get by nvm_get_flash_info()
    @returns pointer to bin content memory, Null failed
*/
void unload_hex(void *dhex_ptr);
hex_data_t *load_hex(const char *file, const flash_info_t *iflash)
{
    hex_data_t hinfo, *dhex = NULL;
    u32 from, to, size, len, off;
    u32 mask = iflash->flash_pagesize - 1;
    int result;

    memset(&hinfo, 0, sizeof(hinfo));
    result = get_hex_info(file, &hinfo);
    if (result) {
        DBG_INFO(UPDI_DEBUG, "get_hex_info failed %d", result);
        return NULL;
    }
    
    //align the data to page size
    from = hinfo.addr_from & ~mask;
    to = ((hinfo.addr_to + mask) & ~mask) - 1;
    size = to - from + 1;
    off = hinfo.addr_from & mask;
    len = size + sizeof(*dhex);
    if (from < iflash->flash_start) {
        from += iflash->flash_start;
        to += iflash->flash_start;
    }

    if (to >= (u32)(iflash->flash_start + iflash->flash_size)) {
        DBG_INFO(UPDI_DEBUG, "hex addr(%04x ~ %04x) over flash size ", from, to);
        return NULL;
    }

    dhex = (hex_data_t *)malloc(len);
    if (!dhex) {
        DBG_INFO(UPDI_DEBUG, "malloc hexi memory(%d) failed", len);
        return NULL;
    }
    memcpy(dhex, &hinfo, sizeof(*dhex));
    dhex->data = (unsigned char *)(dhex + 1);
    dhex->len = size;
    dhex->offset = off;
    memset(dhex->data, 0xff, size);

    result = get_hex_info(file, dhex);
    if (result) {
        DBG_INFO(UPDI_DEBUG, "get_hex_info failed %d", result);
        result = -4;
        goto failed;
    }

    dhex->addr_from = from;
    dhex->addr_to = to;

    return dhex;

failed:
    unload_hex(dhex);
    return NULL;
}

/*
    destory hex memory pointer
    @dhex_ptr: point to memory block by load_hex()
    @no return
*/
void unload_hex(void *dhex_ptr)
{
    if (dhex_ptr) {
        free(dhex_ptr);
    }
}

/*
    Verify the hex memory with flash content
    @nvm_ptr: updi_nvm_init() device handle
    @dhex: point to memory block by load_hex()
    @return 0 mean consistent, other value mismatch
*/
int verify_hex(void *nvm_ptr, hex_data_t *dhex)
{
    u8 * rdata;
    int i, result;

    //compare data
    rdata = malloc(dhex->len);
    if (!rdata) {
        DBG_INFO(UPDI_DEBUG, "malloc rdata failed");
        return -2;
    }

    result = nvm_read_flash(nvm_ptr, dhex->addr_from, rdata, dhex->len);
    if (result) {
        DBG_INFO(UPDI_DEBUG, "nvm_read_flash failed %d", result);
        result = -3;
        goto out;
    }

    for (i = 0; i < dhex->len; i++) {
        if (dhex->data[i] != rdata[i]) {
            DBG_INFO(UPDI_DEBUG, "check flash data failed at %d, %02x-%02x", i, dhex->data[i], rdata[i]);
            break;
        }
    }

    if (i < dhex->len) {
        DBG_INFO(UPDI_DEBUG, "data verifcation failed");
        result = -4;
        goto out;
    }

    DBG(UPDI_DEBUG, "Flash data verified", rdata, dhex->len, "%02x ");

out:
    if (rdata)
        free(rdata);

    return result;
}

/*
    Program flash
    @nvm_ptr: updi_nvm_init() device handle
    @file: Hex file path
    @prog: Whether do program, if not, only verify the content
    @returns 0 - success, other value failed code
*/
int updi_flash(void *nvm_ptr, const char *file, bool prog)
{
    hex_data_t *dhex = NULL;
    flash_info_t flash;
    int result = 0;

    result = nvm_get_flash_info(nvm_ptr, &flash);
    if (result) {
        DBG_INFO(UPDI_DEBUG, "nvm_get_flash_info failed %d", result);
        result = -4;
        goto out;
    }

    dhex = load_hex(file, &flash);
    if (!dhex) {
        DBG_INFO(UPDI_DEBUG, "updi_load_hex failed");
        return -2;
    }

    if (prog) {
        result = nvm_chip_erase(nvm_ptr);
        if (result) {
            DBG_INFO(UPDI_DEBUG, "nvm_chip_erase failed %d", result);
            result = -3;
            goto out;
        }

        result = nvm_write_flash(nvm_ptr, dhex->addr_from, dhex->data, dhex->len);
        if (result) {
            DBG_INFO(UPDI_DEBUG, "nvm_write_flash failed %d", result);
            result = -3;
            goto out;
        }
    }

    result = verify_hex(nvm_ptr, dhex);
    if (result) {
        DBG_INFO(UPDI_DEBUG, "nvm_write_flash failed %d", result);
        result = -3;
        goto out;
    }

    DBG_INFO(UPDI_DEBUG, "Flash check finished");

out:
    unload_hex(dhex);
    return result;
}

/*
    Save flash content to a Hex file
    @nvm_ptr: updi_nvm_init() device handle
    @file: Hex file path for output
    @returns 0 - success, other value failed code
*/
int updi_save(void *nvm_ptr, const char *file)
{
    flash_info_t flash;
    hex_data_t * dhex;
    char * new_file;
    const char *new_file_posfix = ".save";
    size_t size;
    int result = 0;

    result = nvm_get_flash_info(nvm_ptr, &flash);
    if (result) {
        DBG_INFO(UPDI_DEBUG, "nvm_get_flash_info failed %d", result);
        return -2;
    }

    size = flash.flash_size + sizeof(*dhex) + strlen(file) + strlen(new_file_posfix) + 1;
    dhex = (hex_data_t *)malloc(size);
    if (!dhex) {
        DBG_INFO(UPDI_DEBUG, "malloc hexi memory(%d) failed", size);
        return -3;
    }
    memset(dhex, 0, sizeof(*dhex));
    dhex->data = (unsigned char *)(dhex + 1);
    dhex->len = flash.flash_size;
    dhex->offset = 0;
    dhex->total_size = dhex->actual_size = dhex->len;
    dhex->addr_from = 0;
    dhex->addr_to = flash.flash_size - 1;
   
    result = nvm_read_flash(nvm_ptr, flash.flash_start, dhex->data, flash.flash_size);
    if (result) {
        DBG_INFO(UPDI_DEBUG, "nvm_read_flash failed %d", result);
        result = -4;
        goto out;
    }

    new_file = dhex->data + dhex->len;
    new_file[0] = '\0';
    strcat(new_file, file);
    strcat(new_file, new_file_posfix);
    result = save_hex_info(new_file, dhex);
    if (result) {
        DBG_INFO(UPDI_DEBUG, "save_hex_info failed %d", result);
        result = -5;
        goto out;
    }

    DBG_INFO(UPDI_DEBUG, "Saved Hex to \"%s\"", new_file);

out:
    free(dhex);
    return result;
}

/*
    Memory Read
    @nvm_ptr: updi_nvm_init() device handle
    @cmd: cmd string use for address and count. Format: [addr];[count]
    @returns 0 - success, other value failed code
*/
int _updi_read_mem(void *nvm_ptr, char *cmd, void *outbuf, int outlen)
{
    char** tk_s, **tk_w;    //token section, token words
#define UPDI_READ_STROKEN_WORDS_LEN 255
    int address, len, copylen, outlen_left = outlen;
    char *buf;
    int i, j, result = 0;

    tk_s = str_split(cmd, '|');
    if (!tk_s) {
        DBG_INFO(UPDI_DEBUG, "Parse read str tk_s: %s failed", cmd);
        return -2;
    }

    for (i = 0; tk_s[i]; i++) {
        tk_w = str_split(tk_s[i], ';');
        if (!tk_w) {
            DBG_INFO(UPDI_DEBUG, "Parse read str tk_w: %s failed", tk_s[i]);
            result = -2;
            break;
        }

        for (j = 0; tk_w[j]; j++) {
            DBG_INFO(UPDI_DEBUG, "Read[%d]: %s", j, tk_w[j]);

            if (j == 0)
                address = (int)strtol(tk_w[j], NULL, 16);
            else if (j == 1) {
                len = (int)(strtol(tk_w[j], NULL, 10));    //Max size 255 once
                if (len > UPDI_READ_STROKEN_WORDS_LEN) {
                    DBG_INFO(UPDI_DEBUG, "Read memory len %d over max, set to", len, UPDI_READ_STROKEN_WORDS_LEN);
                    len = UPDI_READ_STROKEN_WORDS_LEN;
                }
            }
            free(tk_w[j]);
        }
        free(tk_w);

        if (j == 2) {
            buf = malloc(len);
            if (!buf) {
                DBG_INFO(UPDI_DEBUG, "mallloc memory %d failed", len);
                result = -3;
                break;
            }

            result = nvm_read_mem(nvm_ptr, address, buf, len);
            if (result) {
                DBG_INFO(UPDI_DEBUG, "nvm_read_mem failed %d", result);
                result = -4;
            }
            else {
                if (outbuf && outlen_left > 0) {
                    copylen = min(outlen, len);
                    memcpy(outbuf, buf, copylen);
                    (char *)outbuf += copylen;
                    outlen_left -= copylen;
                }
                else {
                    //Debug output:
                    DBG(DEFAULT_DEBUG, "Read tk[%d]:", buf, len, "%02x ", j);
                }
            }

            free(buf);
        }
        else {
            DBG_INFO(UPDI_DEBUG, "Parse token count num = %d failed", j);
            result = -4;
            break;
        }
    }

    if (result)
        return result;

    return outlen - outlen_left;
}

/*
Memory Read
@nvm_ptr: updi_nvm_init() device handle
@cmd: cmd string use for address and count. Format: [addr];[count]
@returns 0 - success, other value failed code
*/
int updi_read(void *nvm_ptr, char *cmd)
{
    return _updi_read_mem(nvm_ptr, cmd, NULL, 0);
}

/*
    Memory Write
    @nvm_ptr: updi_nvm_init() device handle
    @cmd: cmd string use for address and data. Format: [addr];[dat0];[dat1];[dat2]...
    @returns 0 - success, other value failed code
*/
int updi_write(void *nvm_ptr, char *cmd)
{
    char** tokens;
    int address;
#define UPDI_WRITE_STROKEN_LEN 16
    char buf[UPDI_WRITE_STROKEN_LEN];
    int i, j, len, result = 0;
    bool dirty = false;

    tokens = str_split(cmd, ';');
    if (!tokens) {
        DBG_INFO(UPDI_DEBUG, "Parse write str: %s failed", cmd);
        return -2;
    }

    for (i = 0; tokens[i]; i++) {
        DBG_INFO(UPDI_DEBUG, "Write[%d]: %s", i, tokens[i]);

        if (i == 0) {
            address = (int)strtol(tokens[i], NULL, 16);
        }
        else {
            if (result == 0) {
                j = (i - 1) % UPDI_WRITE_STROKEN_LEN;
                buf[j] = (char)(strtol(tokens[i], NULL, 16) & 0xff);
                dirty = true;
                if (j + 1 == UPDI_WRITE_STROKEN_LEN) {
                    result = nvm_write_mem(nvm_ptr, address + i - j - 1, buf, j + 1);
                    if (result) {
                        DBG_INFO(UPDI_DEBUG, "nvm_write_mem failed %d", result);
                        result = -3;
                    }
                    dirty = false;
                }
            }
        }

        free(tokens[i]);
    }
    free(tokens);

    //write the left data
    if (dirty && result == 0) {
        result = nvm_write_mem(nvm_ptr, address + i - j - 2, buf, j + 1);
        if (result) {
            DBG_INFO(UPDI_DEBUG, "nvm_write_mem failed %d", result);
            result = -4;
        }
    }
    DBG_INFO(DEFAULT_DEBUG, "Write address %x(%d), result %d", address, i - 1, result);

    //readback
    if (result == 0) {
        DBG_INFO(DEFAULT_DEBUG, "Readback address %x(%d):", address, i - 1);
        for (j = 0; j < i - 1; j += UPDI_WRITE_STROKEN_LEN) {
            len = i - j - 1;
            if (len > UPDI_WRITE_STROKEN_LEN)
                len = UPDI_WRITE_STROKEN_LEN;

            result = nvm_read_mem(nvm_ptr, address + j, buf, len);
            if (result) {
                DBG_INFO(UPDI_DEBUG, "nvm_read_mem failed %d", result);
                result = -5;
                break;
            }
            DBG(DEFAULT_DEBUG, "", buf, len, "%02x ");
        }
    }

    return result;
}


/*
Debug view
@nvm_ptr: updi_nvm_init() device handle
@cmd: cmd string use for address and data. Format: ds=[ptr]|dr[ptr]|loop=[dat1]|keys[dat2]
        ds pointer: ptc_qtlib_node_stat1 address, which store structure qtm_acq_node_data_t, we get signal and cc value in it
        dr pointer: qtlib_key_data_set1 address, which store qtm_touch_key_data_t, we get ref value in it
@returns 0 - success, other value failed code. this function will print the log in console
*/

/* ---------------------------------------------------------------------------------------- */
/* Acquisition Node run-time data */
/* ---------------------------------------------------------------------------------------- */
#pragma pack(1)
typedef struct {
    uint8_t  node_acq_status;
    uint16_t node_acq_signals;
    uint16_t node_comp_caps;
} qtm_acq_node_data_t;

/* ---------------------------------------------------------------------------------------- */
/* Key sensor run-time data */
/* ---------------------------------------------------------------------------------------- */
#pragma pack(1)
typedef struct {
    uint8_t              sensor_state;         /* Disabled, Off, On, Filter, Cal... */
    uint8_t              sensor_state_counter; /* State counter */
    uint8_t              node_data_struct_ptr[2]; /* Pointer to node data structure */
    uint16_t             channel_reference;    /* Reference signal */
} qtm_touch_key_data_t;

/* param tag */
enum {
    SIGNAL_ADDR,
    REFERENCE_ADDR,
    LOOP_CNT,
    KEY_CNT,
    MAX_PARAM_NUM
};

const char *token_tag[MAX_PARAM_NUM] = {
    "ds"/*ptc_qtlib_node_stat1->signal and cc value*/,
    "dr" /*qtlib_key_data_set1->ref value*/,
    "loop",
    "keys" };

/*! Byte swap short, 'tiny' chip is little endian, if not match of OS cpu, switch it*/
int16_t swap_int16(int16_t val)
{
#if defined(BIG_ENDIAN)
    return (val << 8) | ((val >> 8) & 0xFF);
#else
    return val;
#endif
}

int updi_debugview(void *nvm_ptr, char *cmd)
{
    char** tk_s, **tk_w;    //token section, token words
    int16_t val, ref_value, signal_value, delta_value;
    double cc_value;
    int i,j, result = 0;
    
    //time varible
    time_t timer;
    char timebuf[26];
    struct tm* tm_info;    

    //debug varible
    qtm_acq_node_data_t ptc_signal;
    qtm_touch_key_data_t ptc_ref;
    int params[MAX_PARAM_NUM] = {0/*SIGNAL_ADDR*/, 0/*REFERENCE_ADDR*/, 0/*LOOP_CNT*/, 0/*KEY_CNT*/}; //loop value default set to 1, keys default set to 1

    //memset(&params, 0, sizeof(params));

    tk_s = str_split(cmd, '|');
    if (!tk_s) {
        DBG_INFO(UPDI_DEBUG, "Parse debugview str tk_s: %s failed", cmd);
        return -2;
    }

    for (i = 0; tk_s[i]; i++) {
        tk_w = str_split(tk_s[i], '=');
        if (!tk_w) {
            DBG_INFO(UPDI_DEBUG, "Parse debugview str tk_w: %s failed", tk_s[i]);
            continue;
        }

        if (tk_w[0] && tk_w[1]) {
            for (j = 0; j < MAX_PARAM_NUM; j++) {
                if (!strcmp(token_tag[j], tk_w[0])) {
                    params[j] = (int)strtol(tk_w[1], NULL, 16);
                    break;
                }
            }
        }
    }

    //if LOOP_CNT less than or qual 0: loop forever
    for (i = 0; params[LOOP_CNT] <= 0 || i < params[LOOP_CNT]; i++) {
        for (j = 0; j < params[KEY_CNT]; j++) {

            memset(&ptc_signal, 0, sizeof(ptc_signal));
            memset(&ptc_ref, 0, sizeof(ptc_ref));

            if (params[SIGNAL_ADDR]) {
                result = nvm_read_mem(nvm_ptr, (u16)(params[SIGNAL_ADDR] + j * sizeof(ptc_signal)), (void *)&ptc_signal, sizeof(ptc_signal));
                if (result) {
                    DBG_INFO(UPDI_DEBUG, "nvm_read_mem signal failed %d", result);
                    result = -4;
                    break;
                }
            }

            if (params[REFERENCE_ADDR]) {
                result = nvm_read_mem(nvm_ptr, (u16)(params[REFERENCE_ADDR] +j * sizeof(ptc_ref)), (void *)&ptc_ref, sizeof(ptc_ref));
                if (result) {
                    DBG_INFO(UPDI_DEBUG, "nvm_read_mem reference failed %d", result);
                    result = -5;
                    break;
                }
            }

            time(&timer);
            tm_info = localtime(&timer);
            strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm_info);

            val = (int16_t)swap_int16(ptc_signal.node_comp_caps);
            cc_value = (val & 0x0F)*0.00675 + ((val >> 4) & 0x0F)*0.0675 + ((val >> 8) & 0x0F)*0.675 + ((val >> 12) & 0x3) * 6.75;
            ref_value = (int16_t)swap_int16(ptc_ref.channel_reference);
            signal_value = (int16_t)swap_int16(ptc_signal.node_acq_signals);
            delta_value = signal_value - ref_value;

            //Debug output:
            DBG_INFO(DEFAULT_DEBUG, "T[%s][%d-%d]: delta,%d, ref,%d, signal,%d, cc,%.2f, sensor_state,%02xH, node_state,%02xH", timebuf, i, j,
                delta_value,
                ref_value,
                signal_value,
                cc_value,
                ptc_ref.sensor_state,
                ptc_signal.node_acq_status);
        }
    }

    return result;
}
