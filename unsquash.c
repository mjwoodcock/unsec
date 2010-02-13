#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compress.h"

static char file_name[1024];
static FILE *infp;

int debug = 0;

struct squash_header
{
	int orig_size;
	int load;
	int exec;
	int attrib;
};

unsigned int
read_uint32()
{
	unsigned int num = 0;

	num = fgetc(infp);
	num |= fgetc(infp) << 8;
	num |= fgetc(infp) << 16;
	num |= fgetc(infp) << 24;

	return num;
}

unsigned int
read_uint16()
{
	unsigned int num = 0;

	num = fgetc(infp);
	num |= fgetc(infp) << 8;

	return num;
}

int
open_squash_file(FILE **fp, char *filename)
{
	char buf[5];
	int i;

	*fp = fopen(filename, "r");
	if (*fp)
	{
		buf[4] = '\0';
		for (i = 0; i < 4; i++)
		{
			buf[i] = fgetc(infp);
		}
		if (strcmp(buf, "SQSH") != 0)
		{
			fclose(*fp);
			*fp = NULL;
			fprintf(stderr, "No a squash file\n");
			return 1;
		}

		return 0;
	}

	fprintf(stderr, "Can not open squash file\n");

	return 2;
}

int
read_squash_header(FILE *fp, struct squash_header *header)
{
	header->orig_size = read_uint32();
	header->load = read_uint32();
	header->exec = read_uint32();
	header->attrib = read_uint32();

	if (feof(fp))
	{
		return 1;
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	int c;
	char out_file_name[1024];
	struct squash_header header;
	unsigned long here, end;
	int cmp_size;
	FILE *fp;
	char file_type[10];
	char *p;

	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s [sec file]\n");
		exit(1);
	}

	if (open_squash_file(&infp, argv[1]))
	{
		exit(1);
	}
	else
	{
		strcpy(out_file_name, argv[1]);
		p = strrchr(out_file_name, ',');
		if (*p)
		{
			*p = '\0';
		}
	}

	read_squash_header(infp, &header);

	// Find out how much compressed data we have got.
	here = ftell(infp);
	fseek(infp, 0, SEEK_END);
	end = ftell(infp);
	cmp_size = end - here;
	fseek(infp, here, SEEK_SET);

	if ((header.load & 0xfff00000) == 0xfff00000) {
		if (debug)
			printf("Filetype = %x\n", (header.load >> 8) & 0xfff);
		snprintf(file_type, sizeof(file_type), ",%03x", (header.load >> 8) & 0xfff);
		strcat(out_file_name, file_type);
	}

	fp = fopen(out_file_name, "w");
	if (fp == NULL)
	{
		printf("Failed to open file for writing\n");
		exit(1);
	}

	uncompress(cmp_size, header.orig_size, infp, fp, UNIX_COMPRESS);

	return 0;
}
