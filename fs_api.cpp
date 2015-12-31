/***
File System Structure

*****************************************************
*   OFT   *   FPL   *            Pages....          *
*****************************************************

OFT: Open file table
FPL: Free page list

Indexed Alloc => A page hold all pages which file need

1M => need 7 pages for OFT
1 pages for FPT => hold 16.5M

OFT stop when addr = -1
FPT stop when flag = -1
***/

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>

#define KB 1024
#define MB (KB*KB)
#define GB (MB*KB)

#define MAX_FILE_NAME_LEN 100
#define MAX_FILE_NUM 1024

#define MAX_FS_NAME_LEN 200
char FS_NAME[100]; // name of the selected file system

#define FPAGE_SIZE 4*KB

const char FS_EXT[4] = ".fs";
unsigned fd_cnt = 0;

typedef struct OFT_element {
	pid_t pid;
	char file_name[MAX_FILE_NAME_LEN];
	int64_t addr;
}OFT;

typedef struct FPT_element {
	int8_t flag;
}FPT;

typedef struct fs_header {
	int32_t oft_pages;
	int32_t fpt_pages;
}FS_HEADER;
FS_HEADER header_info; // read from the start of the fs file


int myfs_create(const char *filesystemname, int max_size);
int myfs_destroy(const char *filesystemname);
int select_fs(const char *filesystemname); // not require
int fs_check_set(); // not require
void init_fs();

int init_OFT(int max_size, FS_HEADER* header);
int init_FPT(int max_size, FS_HEADER* header);

int read_header();
void add_extention(char *target, const char *source);

int main()
{
	myfs_create("my_fs", 1*MB);
	return 0;
}

/*
Init file system
*/
void init_fs()
{

}

/*
Try to create a file with specific size
Return 0 for success else return -1
 */
int myfs_create(const char *filesystemname, int max_size)
{
	char fs_name[MAX_FS_NAME_LEN];
	add_extention(fs_name, filesystemname);
	FILE *fp = fopen(fs_name, "wb");
	if(fp)
	{
		/* DEBUG */
	//	puts("Start create");
		fseek(fp, max_size, SEEK_SET);
		fputc('\0', fp);
		select_fs(filesystemname);

		FS_HEADER header;
		init_OFT(max_size, &header);
		init_FPT(max_size, &header);
		fseek(fp, 0, SEEK_SET);
		fwrite(&header, sizeof(FS_HEADER), 1, fp);

		fclose(fp);
		read_header();
		return 0;
	}
	else
		return -1;
}

/*
Calculate pages for OFT and FPT and set them to header
*/
int init_OFT(int max_size, FS_HEADER* header)
{
	int32_t OFT_pages = (float)max_size/MB*7+1;
	header->oft_pages = OFT_pages;
}

int init_FPT(int max_size, FS_HEADER* header)
{
	int32_t FPT_pages = (float)max_size/16.5/MB;
	header->fpt_pages = FPT_pages;
}

/*
Read header from fs file and set it to header_info var
*/
int read_header()
{
	FILE* fp = fopen(FS_NAME, "rb");
	if(fp)
	{
		fseek(fp, 0, SEEK_SET);
		fread(&header_info, sizeof(FS_HEADER), 1, fp);
		fclose(fp);
		/* DEBUG */
		printf("%d %d\n", header_info.oft_pages, header_info.fpt_pages);
	}
}

/*
Add extention .fs to source
*/
void add_extention(char *target, const char *source)
{
	strcpy(target, source);
	strcat(target, FS_EXT);
}

/*
Try to remove the file with name
Return 0 for success else return -1
*/
int myfs_destroy(const char *filesystemname)
{
	char fs_name[MAX_FS_NAME_LEN];
	add_extention(fs_name, filesystemname);
	return remove(fs_name);
}

/*
Select the file system with fs name
Return 0 if success, else return -1
*/
int select_fs(const char* filesystemname)
{
	/* select default filesystem name if filesystemname is NULL */
	if(filesystemname == NULL)
	{
		DIR *dir;
		struct dirent *ent;
		if ((dir = opendir ("./")) != NULL)
		{
			/* print all the files and directories within directory */
			int flag = 0;
			while ((ent = readdir (dir)) != NULL) 
			{
				if(strstr(ent->d_name, FS_EXT))
				{
					flag = 1;
					strcpy(FS_NAME, ent->d_name);
					break;
				}
			}

			init_fs();
			if(flag)
				return 0;
			else
				return -1;
			closedir (dir);
		} else {
			/* could not open directory */
			perror ("");
			return EXIT_FAILURE;
		}
	}

	char fs_name[MAX_FS_NAME_LEN];
	add_extention(fs_name, filesystemname);
	if(access(fs_name, F_OK)!=-1)
	{
		strcpy(FS_NAME, fs_name);
		init_fs();
		return 0;
	}
	else
		return -1;
}

/*
Check whether the file system is selected
If not select, select it by default and return -1.
Else return 0
*/
int fs_check_set()
{
	if(strlen(FS_NAME))
		return 0;
	else
	{
		select_fs(NULL);
		return -1;
	}
}
