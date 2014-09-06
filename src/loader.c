/*
 * Copyright (C) 2014 - plutoo
 * Copyright (C) 2014 - ichfly
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include "util.h"
#include "arm11.h"
#include "mem.h"
#include "fs.h"

#include "threads.h"
#include "loader.h"

u32 loader_txt = 0;
u32 loader_data = 0;
u32 loader_bss = 0;
extern char* codepath;

extern thread threads[MAX_THREADS];

static u32 Read32(uint8_t p[4])
{
    u32 temp = p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24;
    return temp;
}

static u32 AlignPage(u32 in)
{
    return ((in + 0xFFF) / 0x1000) * 0x1000;
}

u32 GetDecompressedSize(u8* compressed, u32 compressedsize)
{
    u8* footer = compressed + compressedsize - 8;

    u32 originalbottom = Read32(footer+4);
    return originalbottom + compressedsize;
}

int Decompress(u8* compressed, u32 compressedsize, u8* decompressed, u32 decompressedsize)
{
    /*FILE * pFile;
    pFile = fopen("C:\\devkitPro\\code.code", "rb");
    if (pFile != NULL)
    {
        fread(decompressed, 1, decompressedsize, pFile);
        fclose(pFile);
        return 1;
    }*/

    u8* footer = compressed + compressedsize - 8;
    u32 buffertopandbottom = Read32(footer+0);
    u32 i, j;
    u32 out = decompressedsize;
    u32 index = compressedsize - ((buffertopandbottom>>24)&0xFF);
    u32 segmentoffset;
    u32 segmentsize;
    u8 control;
    u32 stopindex = compressedsize - (buffertopandbottom&0xFFFFFF);

    memset(decompressed, 0, decompressedsize);
    memcpy(decompressed, compressed, compressedsize);

    while(index > stopindex) {
        control = compressed[--index];

        for(i=0; i<8; i++) {
            if(index <= stopindex)
                break;
            if(index <= 0)
                break;
            if(out <= 0)
                break;

            if(control & 0x80) {
                if(index < 2) {
                    fprintf(stderr, "Error, compression out of bounds\n");
                    goto clean;
                }

                index -= 2;

                segmentoffset = compressed[index] | (compressed[index+1]<<8);
                segmentsize = ((segmentoffset >> 12)&15)+3;
                segmentoffset &= 0x0FFF;
                segmentoffset += 2;

                if(out < segmentsize) {
                    fprintf(stderr, "Error, compression out of bounds\n");
                    goto clean;
                }

                for(j=0; j<segmentsize; j++) {
                    u8 data;

                    if(out+segmentoffset >= decompressedsize) {
                        fprintf(stderr, "Error, compression out of bounds\n");
                        goto clean;
                    }

                    data  = decompressed[out+segmentoffset];
                    decompressed[--out] = data;
                }
            } else {
                if(out < 1) {
                    fprintf(stderr, "Error, compression out of bounds\n");
                    goto clean;
                }
                decompressed[--out] = compressed[--index];
            }
            control <<= 1;
        }
    }

    return 1;
clean:
    return 0;
}


static u32 LoadElfFile(u8 *addr)
{
    u32 *header = (u32*) addr;
    u32 *phdr = (u32*) (addr + Read32((u8*) &header[7]));
    u32 n = Read32((u8*)&header[11]) &0xFFFF;
    u32 i;
    
    for (i = 0; i < n; i++, phdr += 8) {
        if (phdr[0] != 1) // PT_LOAD
            continue;

        u32 off = Read32((u8*) &phdr[1]);
        u32 dest = Read32((u8*) &phdr[3]);
        u32 filesz = Read32((u8*) &phdr[4]);
        u32 memsz = Read32((u8*) &phdr[5]);

        //round up (this fixes bad malloc implementation in some homebrew)
        memsz = (memsz + 0xFFF)&~0xFFF;

        u8* data = malloc(memsz);
        memset(data, 0, memsz);
        memcpy(data, addr + off, filesz);
        mem_AddSegment(dest, memsz, data);

        if ((phdr[6] & 0x5) == 0x5)loader_txt = dest; //read execute
        if ((phdr[6] & 0x6) == 0x6)loader_data = loader_bss = dest;//read write
    }

    return Read32((u8*) &header[6]);
}

static void CommonMemSetup()
{
    // Add thread command buffer.

    for (int i = 0; i < MAX_THREADS; i++) {
        threads[i].servaddr = 0xFFFF0000 - (0x1000 * (MAX_THREADS-i)); //there is a mirrow of that addr in wrap.c fix this addr as well if you fix this
    }

    mem_AddSegment(0xFFFF0000 - 0x1000 * MAX_THREADS, 0x1000 * (MAX_THREADS+1), NULL);

    // Add Read Only Shared Info
    mem_AddSegment(0x1FF80000, 0x100, NULL);
    mem_Write8(0x1FF80014, 1); //Bit0 set for Retail
    mem_Write32(0x1FF80040, 64 * 1024 * 1024); //Set App Memory Size to 64MB?

    //Shared Memory Page For ARM11 Processes
    mem_AddSegment(0x1FF81000, 0x100, NULL);
    mem_Write8(0x1FF800c0, 1); //headset connected

}

exheader_header ex;

int loader_LoadFile(FILE* fd)
{
    u32 ncch_off = 0;

    // Read header.
    ctr_ncchheader h;
    if (fread(&h, sizeof(h), 1, fd) != 1) {
        ERROR("failed to read header.\n");
        return 1;
    }

    // Load NCSD
    if (memcmp(&h.magic, "NCSD", 4) == 0) {
        ncch_off = 0x4000;
        fseek(fd, ncch_off, SEEK_SET);
        if (fread(&h, sizeof(h), 1, fd) != 1) {
            ERROR("failed to read header.\n");
            return 1;
        }
    }

    if (Read32(h.signature) == 0x464c457f) { // Load ELF
        // Add stack segment.
        mem_AddSegment(0x10000000 - 0x100000, 0x100000, NULL);

        // Load elf.
        fseek(fd, 0, SEEK_END);
        u32 elfsize = ftell(fd);
        u8* data = malloc(elfsize);
        fseek(fd, 0, SEEK_SET);
        fread(data, elfsize, 1, fd);

        // Set entrypoint and stack ptr.
        arm11_SetPCSP(LoadElfFile(data), 0x10000000);
        CommonMemSetup();

        free(data);
        return 0;
    }

    if (Read32(h.signature) == CIA_MAGIC) { // Load CIA
        cia_header* ch = (cia_header*)&h;

        ncch_off = 0x20 + ch->hdr_sz;

        ncch_off += ch->cert_sz;
        ncch_off += ch->tik_sz;
        ncch_off += ch->tmd_sz;

        ncch_off = (u32)(ncch_off & ~0xff);
        ncch_off += 0x100;
        fseek(fd, ncch_off, SEEK_SET);

        if (fread(&h, sizeof(h), 1, fd) != 1) {
            ERROR("failed to read header.\n");
            return 1;
        }
    }
    // Load NCCH
    if (memcmp(&h.magic, "NCCH", 4) != 0) {
        ERROR("invalid magic.. wrong file?\n");
        return 1;
    }

    // Read Exheader.
    if (fread(&ex, sizeof(ex), 1, fd) != 1) {
        ERROR("failed to read exheader.\n");
        return 1;
    }

    bool is_compressed = ex.codesetinfo.flags.flag & 1;
    //ex.codesetinfo.name[7] = '\0';
    u8 namereal[9];
    strncpy(namereal, ex.codesetinfo.name, 9);
    DEBUG("Name: %s\n", namereal);
    DEBUG("Code compressed: %s\n", is_compressed ? "YES" : "NO");

    // Read ExeFS.
    u32 exefs_off = Read32(h.exefsoffset) * 0x200;
    u32 exefs_sz = Read32(h.exefssize) * 0x200;
    DEBUG("ExeFS offset:    %08x\n", exefs_off);
    DEBUG("ExeFS size:      %08x\n", exefs_sz);

    fseek(fd, exefs_off + ncch_off, SEEK_SET);

    exefs_header eh;
    if (fread(&eh, sizeof(eh), 1, fd) != 1) {
        ERROR("failed to read ExeFS header.\n");
        return 1;
    }

    u32 i;
    for (i = 0; i < 8; i++) {
        u32 sec_size = Read32(eh.section[i].size);
        u32 sec_off = Read32(eh.section[i].offset);

        if (sec_size == 0)
            continue;

        DEBUG("ExeFS section %d:\n", i);
        eh.section[i].name[7] = '\0';
        DEBUG("    name:   %s\n", eh.section[i].name);
        DEBUG("    offset: %08x\n", sec_off);
        DEBUG("    size:   %08x\n", sec_size);


        bool isfirmNCCH = false;

        if (strcmp((char*) eh.section[i].name, ".code") == 0) {
            sec_off += exefs_off + sizeof(eh);
            fseek(fd, sec_off + ncch_off, SEEK_SET);

            uint8_t* sec = malloc(AlignPage(sec_size));
            if (sec == NULL) {
                ERROR("section malloc failed.\n");
                return 1;
            }

            if (fread(sec, sec_size, 1, fd) != 1) {
                ERROR("section fread failed.\n");
                return 1;
            }

            // Decompress first section if flag set.
            if (i == 0 && is_compressed) {
                u32 dec_size = GetDecompressedSize(sec, sec_size);
                u8* dec = malloc(AlignPage(dec_size));

                u32 firmexpected = Read32(ex.codesetinfo.text.codesize) + Read32(ex.codesetinfo.ro.codesize) + Read32(ex.codesetinfo.data.codesize);

                DEBUG("Decompressing..\n");
                if (Decompress(sec, sec_size, dec, dec_size) == 0) {
                    ERROR("section decompression failed.\n");
                    return 1;
                }
                DEBUG("  .. OK\n");


                FILE * pFile;
                pFile = fopen("decode.code", "wb");
                if (pFile != NULL) {
                    fwrite(dec, 1, dec_size, pFile);
                    fclose(pFile);
                }

                if (codepath != NULL) {
                    pFile = fopen(codepath, "rb");
                    if (pFile != NULL) {
                        fread(dec, 1, dec_size, pFile);
                        fclose(pFile);
                    }
                }

                if (dec_size == firmexpected) {
                    isfirmNCCH = true;
                    DEBUG("firm NCCH detected\n");
                }

                free(sec);
                sec = dec;
                sec_size = dec_size;
            }

            // Load .code section.
            if (isfirmNCCH) {

                mem_AddSegment(Read32(ex.codesetinfo.text.address), Read32(ex.codesetinfo.text.codesize), sec);
                mem_AddSegment(Read32(ex.codesetinfo.ro.address), Read32(ex.codesetinfo.ro.codesize), sec + Read32(ex.codesetinfo.text.codesize));

                u8* temp = malloc(Read32(ex.codesetinfo.bsssize) + Read32(ex.codesetinfo.data.codesize));
                memcpy(temp, sec + Read32(ex.codesetinfo.ro.codesize) + Read32(ex.codesetinfo.text.codesize), Read32(ex.codesetinfo.data.codesize));


                mem_AddSegment(Read32(ex.codesetinfo.data.address), Read32(ex.codesetinfo.data.codesize) + Read32(ex.codesetinfo.bsssize), temp);
                free(temp);
            } else {
                sec_size = AlignPage(sec_size);
                mem_AddSegment(Read32(ex.codesetinfo.text.address), sec_size, sec);
                // Add .bss segment.
                u32 bss_off = AlignPage(Read32(ex.codesetinfo.data.address) +
                                        Read32(ex.codesetinfo.data.codesize));
                mem_AddSegment(bss_off, AlignPage(Read32(ex.codesetinfo.bsssize)), NULL);
            }
            loader_txt = Read32(ex.codesetinfo.text.address);
            loader_data = Read32(ex.codesetinfo.ro.address);
            loader_bss = Read32(ex.codesetinfo.data.address);
            free(sec);
        }
    }

    if (Read32(h.romfsoffset) != 0 && Read32(h.romfssize) != 0) {
        u32 romfs_off = ncch_off + (Read32(h.romfsoffset) * 0x200) + 0x1000;
        u32 romfs_sz = (Read32(h.romfssize) * 0x200) - 0x1000;

        DEBUG("RomFS offset:    %08x\n", romfs_off);
        DEBUG("RomFS size:      %08x\n", romfs_sz);

        romfs_Setup(fd, romfs_off, romfs_sz);
    }

    // Add stack segment.
    u32 stack_size = Read32(ex.codesetinfo.stacksize);
    mem_AddSegment(0x10000000 - stack_size, stack_size, NULL);

    // Set entrypoint and stack ptr.
    arm11_SetPCSP(Read32(ex.codesetinfo.text.address),
                  0x10000000);

    CommonMemSetup();
    return 0;
}
