#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compress.h"

struct header
{
	unsigned int check_code1;
	unsigned int check_code2;
	unsigned int total_size;
	unsigned int zero;
	unsigned int thirty_two;
	unsigned int data_offset;
};

#define STATE_NULL 0
#define STATE_GOT_R 1
#define STATE_GOT_RN 2
#define STATE_GOT_RNA 3
#define STATE_GOT_RNAM 4
#define STATE_GOT_RD 5
#define STATE_GOT_RDI 6
#define STATE_GOT_RDIR 7
#define STATE_GOT_RS 8
#define STATE_GOT_RSQ 9
#define STATE_GOT_RSQS 10
#define STATE_GOT_RDA 12
#define STATE_GOT_RDAT 13

#define DATA_OFFSET_POINTER 24

static int debug = 0;
static int list_contents = 0;
static int append_filetype = 1;

static char file_name[1024];
static FILE *infp;

void
usage(char *progname)
{
	fprintf(stderr, "Usage: %s [-l] [-t] sec_file\n", progname);
	fprintf(stderr, "  -l: list contents, don't extract\n");
	fprintf(stderr, "  -t: don't append RISC OS filetype\n");

	exit(1);
}

unsigned int
read_word()
{
	unsigned int num = 0;

	num = fgetc(infp);
	num |= fgetc(infp) << 8;
	num |= fgetc(infp) << 16;
	num |= fgetc(infp) << 24;

	return num;
}

int
open_archive_file(FILE **fp, char *file_name, struct header *header)
{
	*fp = fopen(file_name, "rb");
	if (!*fp)
	{
		return 1;
	}

	read_word();
	header->check_code1 = read_word();
	header->check_code2 = read_word();
	header->total_size = read_word();
	header->zero = read_word();
	header->thirty_two = read_word();
	header->data_offset = read_word();

	if (header->check_code1 != 0x79766748)
	{
		return 2;
	}
	if (header->check_code2 != 0x216c6776)
	{
		return 2;
	}
	if (header->zero != 0)
	{
		return 2;
	}
	if (header->thirty_two != 32)
	{
		return 2;
	}

	return 0;
}

int
skip_to_data(struct header *header)
{
	int r;
	int c;
	unsigned int ptr;

	r = fseek(infp, header->data_offset - DATA_OFFSET_POINTER, SEEK_SET);
	if (r != 0)
	{
		return 1;
	}
	else
	{
		do
		{
			c = fgetc(infp);
		} while (!feof(infp) && c != '\0');
	}

	if (feof(infp))
	{
		return 2;
	}

	return r;
}

void
file_name_to_unix()
{
	char *bufp = &file_name[0];

	while (*bufp)
	{
		if (*bufp == '.')
		{
			*bufp = '/';
		}
		else if (*bufp == '/')
		{
			*bufp = '.';
		}
		*bufp++;
	}
}

void
handle_dir_data()
{
	int c;
	int chunk_size = 0;
	int attributes = 0;

	chunk_size = read_word();
	attributes = read_word();
	if (debug)
		printf("Got dir %s %x\n", file_name, attributes);

	if (!list_contents)
	{
		mkdir(file_name, 0777);
	}
}

void
handle_file_name()
{
	int c;
	int chunk_size;
	int i;
	char *bufp = &file_name[0];

	*bufp = '\0';

	chunk_size = read_word();

	bufp = &file_name[0];
	do {
		*bufp++ = (char)fgetc(infp);
		*bufp++ = (char)fgetc(infp);
		*bufp++ = (char)fgetc(infp);
		*bufp++ = (char)fgetc(infp);
	} while (*(bufp - 1) != '\0');

	file_name_to_unix();

	if (debug)
		printf("Got file %x %s\n", chunk_size, file_name);
}

void
handle_sqs()
{
	int chunk_size;

	chunk_size = read_word();

	if (debug)
		printf("Got sqs %x\n", chunk_size);
}

void
extract_data()
{
	int chunk_size;
	int c;
	unsigned int load;
	unsigned int exec;
	unsigned int attrs;
	unsigned int cmp_size;
	unsigned int orig_size;
	int i;
	FILE *fp;
	char file_type[10];

	chunk_size = read_word();
	if (debug)
		printf("Got data %d\n", chunk_size);

	load = read_word();
	if (append_filetype &&((load & 0xfff00000) == 0xfff00000))
	{
		if (debug)
			printf("filetype = %x\n", (load >> 8) & 0xfff);
		snprintf(file_type, sizeof(file_type), ",%03x", (load >> 8) & 0xfff);
		strcat(file_name, file_type);
	}

	exec = read_word();
	if (debug)
		printf("exec = %x\n", exec);
	attrs = read_word();
	if (debug)
		printf("attrs = %x\n", attrs);
	cmp_size = read_word();
	if (debug)
		printf("compressed size =  %d\n", cmp_size);
	orig_size = read_word();
	if (debug)
		printf("original size =  %d\n", orig_size);

	if (!list_contents)
	{
		if ((fp = fopen(file_name, "wb")) == NULL)
		{
			printf("Failed to open file %s\n", file_name);
		}
	}

	if (list_contents)
	{
		fseek(infp, chunk_size - 20, SEEK_CUR);
		printf("%8d %s\n", orig_size, file_name);
	}
	else if (cmp_size == 0)
	{
		for (i = 0; i < orig_size; i++)
		{
			c = fgetc(infp);
			fputc(c, fp);
		}
	}
	else
	{
		uncompress(cmp_size, orig_size, infp, fp, UNIX_COMPRESS);
	}

	if (!list_contents)
	{
		fclose(fp);
	}
	// FIXME: read to end of 4 byte block
}

int
main(int argc, char *argv[])
{
	int c;
	int state = STATE_NULL;
	int r;
	struct header header;
	int i;
	char *file_name;

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			if (strcmp(argv[i], "-l") == 0)
			{
				list_contents = 1;
			}
			else if (strcmp(argv[i], "-t") == 0)
			{
				append_filetype = 0;
			}
			else if (strcmp(argv[i], "-h") == 0)
			{
				usage(argv[0]);
			}
		}
		else
		{
			file_name = argv[i];
		}
	}

	if (argc < 2 || file_name == NULL)
	{
		usage(argv[0]);
	}

	r = open_archive_file(&infp, file_name, &header);
	if (r != 0)
	{
		switch(r)
		{
		case 1:
			fprintf(stderr, "Can not open %s for reading\n", file_name);
			break;
		case 2:
			fprintf(stderr, "Bad archive header\n");
			break;
		}
		exit(1);
	}

	r = skip_to_data(&header);
	if (r != 0)
	{
		switch (r)
		{
		case 1:
			fprintf(stderr, "Can not find compressed data\n");
			break;
		case 2:
			fprintf(stderr, "Unexpected end of file\n");
			break;
		}
		exit(1);
	}

	while ((c = fgetc(infp)) != EOF)
	{
		switch (state)
		{
		case STATE_NULL:
			if (c == 'r')
			{
				state = STATE_GOT_R;
			}
			break;
		case STATE_GOT_R:
			if (c == 'd')
			{
				state = STATE_GOT_RD;
			}
			else if (c == 'n')
			{
				state = STATE_GOT_RN;
			}
			else if (c == 's')
			{
				state = STATE_GOT_RS;
			}
			else
			{
				state = STATE_NULL;
			}
			break;
		case STATE_GOT_RD:
			if (c == 'i')
			{
				state = STATE_GOT_RDI;
			}
			else if (c == 'a')
			{
				state = STATE_GOT_RDA;
			}
			else
			{
				state = STATE_NULL;
			}
			break;
		case STATE_GOT_RN:
			if (c == 'a')
			{
				state = STATE_GOT_RNA;
			}
			else
			{
				state = STATE_NULL;
			}
			break;
		case STATE_GOT_RS:
			if (c == 'q')
			{
				state = STATE_GOT_RSQ;
			}
			else
			{
				state = STATE_NULL;
			}
			break;
		case STATE_GOT_RDI:
			if (c == 'r')
			{
				handle_dir_data();
			}
			state = STATE_NULL;
			break;
		case STATE_GOT_RNA:
			if (c == 'm')
			{
				handle_file_name();
			}
			state = STATE_NULL;
			break;
		case STATE_GOT_RDA:
			if (c == 't')
			{
				extract_data();
			}
			state = STATE_NULL;
			break;
		case STATE_GOT_RSQ:
			if (c == 's')
			{
				handle_sqs();
			}
			state = STATE_NULL;
			break;
		}
	}

	return 0;
}
