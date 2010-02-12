#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compress.h"

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

static int debug = 0;
static int done_header = 0;

static char file_name[1024];

unsigned int
read_word()
{
	unsigned int num = 0;

	num = fgetc(stdin);
	num |= fgetc(stdin) << 8;
	num |= fgetc(stdin) << 16;
	num |= fgetc(stdin) << 24;

	return num;
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
print_dir_name()
{
	int c;
	int num = 0;
	unsigned long here;

	here = ftell(stdin);
	done_header++;
	if (done_header < 4)
	{
		return;
	}

	num = read_word();
	if (debug)
		printf("Got dir %ld %x\n", here, num);
	read_word(); // I'm not sure what this word is for, yet
}

void
print_file_name()
{
	int c;
	int num;
	int i;
	unsigned long here;
	char *bufp = &file_name[0];

	here = ftell(stdin);
	done_header++;
	if (done_header < 4)
	{
		return;
	}

	num = read_word();

	bufp = &file_name[0];
	do {
		*bufp++ = (char)fgetc(stdin);
		*bufp++ = (char)fgetc(stdin);
		*bufp++ = (char)fgetc(stdin);
		*bufp++ = (char)fgetc(stdin);
	} while (*(bufp - 1) != '\0');
	file_name_to_unix();
	if (debug)
		printf("Got file %ld %x %s\n", here, num, file_name);
}

void
print_seq()
{
	int num;

	done_header++;
	if (done_header < 4)
	{
		return;
	}

	num = read_word();

	if (debug)
		printf("Got seq %x\n", num);
}

void
create_dir()
{
	char dir_name[1024];
	char cmd[1024];
	char *p;

	strcpy(dir_name, file_name);
	p = strrchr(dir_name, '/');
	if (p)
	{
		*p = '\0';
	}
	snprintf(cmd, sizeof(cmd), "mkdir -p %s\n", dir_name);
	system(cmd);
}

void
extract_data()
{
	int num;
	int c;
	unsigned long here;
	unsigned int load;
	unsigned int exec;
	unsigned int attrs;
	unsigned int cmp_size;
	unsigned int orig_size;
	int i;
	FILE *fp;
	char file_type[10];

	done_header++;
	if (done_header < 4)
	{
		return;
	}

	here = ftell(stdin);
	num = read_word();
	if (debug)
		printf("Got data %ld %d\n", here, num);

	load = read_word();
	if ((load & 0xfff00000) == 0xfff00000) {
		if (debug)
			printf("Filetype = %x\n", (load >> 8) & 0xfff);
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

	create_dir();
	if ((fp = fopen(file_name, "w")) == NULL)
	{
		printf("Failed to open file %s\n", file_name);
	}
	if (cmp_size == 0)
	{
		for (i = 0; i < orig_size; i++)
		{
			c = fgetc(stdin);
			fputc(c, fp);
		}
	}
	else
	{
		uncompress(cmp_size, orig_size, stdin, fp, UNIX_COMPRESS);
		printf("\n");
	}
	fclose(fp);
	// Fixme: read to end of 4 byte block
}

int
main(int argc, char *argv[])
{
	int c;
	int state = STATE_NULL;

	while ((c = fgetc(stdin)) != EOF)
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
				print_dir_name();
			}
			state = STATE_NULL;
			break;
		case STATE_GOT_RNA:
			if (c == 'm')
			{
				print_file_name();
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
				print_seq();
			}
			state = STATE_NULL;
			break;
		}
	}

	return 0;
}
