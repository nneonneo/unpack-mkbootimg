#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <sys/stat.h>

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

void write_string_to_file(char *base, char *suffix, char *string) {
    char file[PATH_MAX];
    sprintf(file, "%s-%s", base, suffix);
    FILE* f = fopen(file, "w");
    if(!f) {
        fprintf(stderr, "failed to write file %s: %s\n", file, strerror(errno));
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
    printf("usage: unpackbootimg boot.img [outdir]\n");
    printf("\t[-p|--pagesize <size-in-hexadecimal>]\n");
    return 1;
}

int main(int argc, char** argv)
{
    char base[PATH_MAX];
    char buf[4096];
    char* directory = "./";
    char* filename = NULL;
    int pagesize = 0;
    int curarg = 1;
    int posarg = 0;

    while(curarg < argc){
        char *arg = argv[curarg++];

        if(!strcmp(arg, "--input") || !strcmp(arg, "-i")) {
            if(curarg >= argc)
                return usage();
            filename = argv[curarg++];
            posarg = 1;
        } else if(!strcmp(arg, "--output") || !strcmp(arg, "-o")) {
            if(curarg >= argc)
                return usage();
            directory = argv[curarg++];
            posarg = 2;
        } else if(!strcmp(arg, "--pagesize") || !strcmp(arg, "-p")) {
            if(curarg >= argc)
                return usage();
            pagesize = strtoul(argv[curarg++], 0, 16);
        } else if(posarg == 0) {
            filename = arg;
            posarg++;
        } else if(posarg == 1) {
            directory = arg;
            posarg++;
        } else {
            return usage();
        }
    }

    if (filename == NULL) {
        return usage();
    }

    mkdir(directory, 0777);

    FILE* f = fopen(filename, "rb");
    boot_img_hdr header;

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

    /* Check for Loki-patched image (https://github.com/djrbliss/loki/) */
    int is_loki = 0;
    fseek(f, i+1024, SEEK_SET);
    fread(buf, 4, 1, f);
    if(memcmp(buf, "LOKI", 4) == 0) {
        printf("Loki-ized image detected.\n");
        is_loki = 1;
    }
    fseek(f, i, SEEK_SET);

    fread(&header, sizeof(header), 1, f);
    /* Fix up Loki-patched image if needed */
    if(is_loki) {
        header.kernel_size = header.dt_size;
        header.dt_size = 0;
        header.ramdisk_size = header.unused;
        header.unused = 0;
        header.ramdisk_addr = header.second_addr;
        header.second_addr = 0;
    }

    printf("BOARD_KERNEL_CMDLINE %s\n", header.cmdline);
    printf("BOARD_KERNEL_BASE %08x\n", header.kernel_addr - 0x00008000);
    printf("BOARD_PAGE_SIZE %d\n", header.page_size);
    
    if (pagesize == 0) {
        pagesize = header.page_size;
    }

    unsigned baseaddr = header.kernel_addr - 0x00008000;

    sprintf(base, "%s/%s", directory, basename(filename));

    write_string_to_file(base, "cmdline", (char *)header.cmdline);

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