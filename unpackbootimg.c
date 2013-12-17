#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>

#include "mincrypt/sha.h"
#include "bootimg.h"

typedef unsigned char byte;

int read_padding(FILE* f, unsigned itemsize, int pagesize)
{
    byte* buf = (byte*)malloc(sizeof(byte) * pagesize);
    unsigned pagemask = pagesize - 1;
    unsigned count;

    if((itemsize & pagemask) == 0) {
        free(buf);
        return 0;
    }

    count = pagesize - (itemsize & pagemask);

    fread(buf, count, 1, f);
    free(buf);
    return count;
}

void write_string_to_file(char* base, char *suffix, char* string) {
    char file[PATH_MAX];
    sprintf(file, "%s-%s", base, suffix);
    FILE* f = fopen(file, "w");
    if(!f) {
        perror("failed to write file");
        exit(-1);
    }
    fwrite(string, strlen(string), 1, f);
    fwrite("\n", 1, 1, f);
    fclose(f);
}

void write_chunk_to_file(FILE *infile, char *base, char *suffix, unsigned size) {
    char outfn[PATH_MAX];
    sprintf(outfn, "%s-%s", base, suffix);
    FILE *outfile = fopen(outfn, "wb");

    if(!outfile) {
        perror("failed to write file");
        exit(-1);
    }

    byte *data = malloc(size);
    fread(data, size, 1, infile);
    fwrite(data, size, 1, outfile);
    fclose(outfile);
    free(data);
}

int usage() {
    printf("usage: unpackbootimg\n");
    printf("\t-i|--input boot.img\n");
    printf("\t[ -o|--output output_directory]\n");
    printf("\t[ -p|--pagesize <size-in-hexadecimal> ]\n");
    return 0;
}

int main(int argc, char** argv)
{
    char base[PATH_MAX];
    char buf[4096];
    char* directory = "./";
    char* filename = NULL;
    int pagesize = 0;

    argc--;
    argv++;
    while(argc > 0){
        char *arg = argv[0];
        char *val = argv[1];
        argc -= 2;
        argv += 2;
        if(!strcmp(arg, "--input") || !strcmp(arg, "-i")) {
            filename = val;
        } else if(!strcmp(arg, "--output") || !strcmp(arg, "-o")) {
            directory = val;
        } else if(!strcmp(arg, "--pagesize") || !strcmp(arg, "-p")) {
            pagesize = strtoul(val, 0, 16);
        } else {
            return usage();
        }
    }
    
    if (filename == NULL) {
        return usage();
    }

    FILE* f = fopen(filename, "rb");
    boot_img_hdr header;

    //printf("Reading header...\n");
    int i;
    for (i = 0; i <= 512; i++) {
        fseek(f, i, SEEK_SET);
        fread(buf, BOOT_MAGIC_SIZE, 1, f);
        if (memcmp(buf, BOOT_MAGIC, BOOT_MAGIC_SIZE) == 0)
            break;
    }
    if (i > 512) {
        printf("Android boot magic not found.\n");
        return 1;
    }
    fseek(f, i, SEEK_SET);
    printf("Android magic found at: %d\n", i);

    fread(&header, sizeof(header), 1, f);
    printf("BOARD_KERNEL_CMDLINE %s\n", header.cmdline);
    printf("BOARD_KERNEL_BASE %08x\n", header.kernel_addr - 0x00008000);
    printf("BOARD_PAGE_SIZE %d\n", header.page_size);
    
    if (pagesize == 0) {
        pagesize = header.page_size;
    }

    unsigned baseaddr = header.kernel_addr - 0x00008000;

    sprintf(base, "%s/%s", directory, basename(filename));

    write_string_to_file(base, "cmdline", header.cmdline);

    sprintf(buf, "%08x", baseaddr);
    write_string_to_file(base, "base", buf);

    sprintf(buf, "--kernel_offset %08x --ramdisk_offset %08x --second_offset %08x --tags_offset %08x",
        header.kernel_addr - baseaddr, header.ramdisk_addr - baseaddr, header.second_addr - baseaddr, header.tags_addr - baseaddr);
    write_string_to_file(base, "offsets", buf);

    sprintf(buf, "%d", header.page_size);
    write_string_to_file(base, "pagesize", buf);

    read_padding(f, sizeof(header), pagesize);


    write_chunk_to_file(f, base, "zImage", header.kernel_size);
    read_padding(f, header.kernel_size, pagesize);

    write_chunk_to_file(f, base, "ramdisk.gz", header.ramdisk_size);
    read_padding(f, header.ramdisk_size, pagesize);

    if(header.second_size) {
        write_chunk_to_file(f, base, "second", header.second_size);
        read_padding(f, header.second_size, pagesize);
    }

    if(header.dt_size) {
        write_chunk_to_file(f, base, "dt", header.dt_size);
        read_padding(f, header.dt_size, pagesize);
    }

    fclose(f);

    return 0;
}