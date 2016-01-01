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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>
#include <vector>
#include <iostream>
using namespace std;

#define KB 1024
#define MB (KB*KB)
#define GB (MB*KB)

#define MAX_FILE_NAME_LEN 100
#define MAX_FILE_NUM 1024

#define MAX_FS_NAME_LEN 200
char FS_NAME[100]; // name of the selected file system

#define PAGE_SIZE (4*KB)

const char FS_EXT[4] = ".fs";
unsigned fd_cnt = 0; // only increase...
map<int32_t, int32_t> ppt;	// per-process table

typedef struct OFT_element {
	char filename[MAX_FILE_NAME_LEN];
	int32_t index;
}OFT;

typedef struct FPT_element {
	int8_t flag;
}FPT;

typedef struct fs_header {
	int32_t oft_pages;
	int64_t oft_startadd;
	int32_t oft_entry_cnt;
	int32_t fpt_pages;
	int64_t fpt_startadd;
	int32_t fpt_entry_cnt;
	int64_t fpage_startadd;
}FS_HEADER;
FS_HEADER header_info; // read from the start of the fs file

typedef struct index_content {
	int32_t page_number;
	int32_t remain;
	vector<int32_t> page_list;
}index_page;

OFT *oft_ptr = NULL; 
FPT *fpt_ptr = NULL;
size_t oft_len, fpt_len;

int myfs_create(const char *filesystemname, int max_size);
int myfs_destroy(const char *filesystemname);
int select_fs(const char *filesystemname); // not require
void init_fs();
int init_OFT(int max_size, FS_HEADER* header);
int init_FPT(int max_size, FS_HEADER* header);
int read_header();
void add_extention(char *target, const char *source);
int32_t getFreePage(); // wb
void update_info();
void header_wb(); // wb
int32_t oft_addEntry(const char* filename); // wb
int32_t getPOMap(int32_t index); // work
int64_t page2addr(int32_t index);
int32_t getIndexMap(int32_t index);

int myfs_file_open(const char* filename); // empty now
int myfs_file_create(const char* filename);
int myfs_file_delete(const char* filename);

void index_page_wb(int32_t oft_index, index_page *ip);



int allocNewPage(int32_t index);
void read_indexPage(int32_t index, index_page *ip);

void showFiles();

int main()
{
	//myfs_create("my_fs", 1*MB);
	myfs_file_create("Good");
	//myfs_file_open("Hello");
	showFiles();
	return 0;
}

/* show all files in fs */
void showFiles()
{
	init_fs();
	FILE* fp = fopen(FS_NAME, "rb+");
	fseek(fp, header_info.oft_startadd, SEEK_SET);
	OFT temp;
	for(int i=0; i<header_info.oft_entry_cnt; i++)
	{
		fread(&temp, sizeof(OFT), 1, fp);
		//printf("read %s\n", temp.filename);
		printf("%d\t%s\n", i, temp.filename);
	}
	fclose(fp);
}

/* write header_info to fs */
void header_wb()
{
	FILE* fp = fopen(FS_NAME, "rb+");
	fseek(fp, 0, SEEK_SET);
	fwrite(&header_info, sizeof(FS_HEADER), 1, fp);
	fclose(fp);
	update_info();
}

/* open a file in fs and return the file descriptor */
int myfs_file_open(const char* filename)
{
	init_fs();
	/* DEBUG */
	printf("check oft_startadd = %ld\n", header_info.oft_startadd);
	printf("create file with desc %d to %d\n", fd_cnt, ppt[fd_cnt]);
	printf("first entry is name: %s and page index: %d\n", oft_ptr[0].filename, oft_ptr[0].index);
	printf("header oft entry cnt: %d\n", header_info.oft_entry_cnt);
}

/* 
return ppt to opt map index.
if the index is invalid => return -1
 */
int32_t getPOMap(int32_t index)
{
	if(ppt.find(index)==ppt.end())
		return -1;
	else
		return ppt[index];
}

/*
return index page address
*/
int32_t getIndexMap(int32_t index)
{
	return oft_ptr[index].index;
}

/*
add new entry to OFT, return the index of free page
if retuen value is -1 => no more place
*/
// my oft can't recycle even delete data...
int32_t oft_addEntry(const char* filename) 
{
	int64_t addr_offset = header_info.oft_startadd+header_info.oft_entry_cnt*sizeof(OFT); // calculate the addr of new entry
	OFT entry;
	entry.index = getFreePage();	// assign free page for indexed page
	strcpy(entry.filename, filename);
	FILE* fp = fopen(FS_NAME, "rb+"); // write new entry to fs file
	fseek(fp, addr_offset, SEEK_SET);
	//printf("temp entry => name: %s, index: %d\n", entry.filename, entry.index);
	fwrite(&entry, sizeof(OFT), 1, fp);
	fclose(fp);
	int32_t ret = header_info.oft_entry_cnt++; // write header to fs file
	header_wb();
	update_info();
	return ret; // return entry index of oft
}

/* 
create a file in fs and return the file descriptor 
*/
int myfs_file_create(const char* filename)
{
	init_fs();
	int32_t map_oft_index = oft_addEntry(filename); 
	ppt[++fd_cnt] = map_oft_index;

	/* init new page for index page */
	int32_t index = getFreePage();
	index_page ip;
	ip.page_number = 1;
	ip.remain = PAGE_SIZE;
	ip.page_list.push_back(index);
	int64_t addr = page2addr(getIndexMap(map_oft_index));
	//cout << "check addr " << addr << ", check fpage_start " << header_info.fpage_startadd << endl;
	index_page_wb(map_oft_index, &ip);
	ip.page_number = 0;
	allocNewPage(map_oft_index);

	/* DEBUG */
	/*
	printf("check oft_startadd = %ld\n", header_info.oft_startadd);
	printf("create file with desc %d to %d\n", fd_cnt, ppt[fd_cnt]);
	//printf("first entry is name: %s and page index: %d\n", oft_ptr[2].filename, oft_ptr[2].index);
	printf("header oft entry cnt: %d\n", header_info.oft_entry_cnt);
	*/
	
	return fd_cnt; // pp desc
}

/* alloc new page to specific oft index page */
int allocNewPage(int32_t oft_index)
{
	int32_t index = getFreePage();
	if(index<0)
		return index; // no space
	index_page ip;
	read_indexPage(oft_index, &ip);
	ip.page_number++;
	ip.remain = PAGE_SIZE;
	ip.page_list.push_back(index);
	index_page_wb(oft_index, &ip);
	/* DEBUG */
	//read_indexPage(oft_index, &ip);
}

/* write index page data */
void index_page_wb(int32_t oft_index, index_page *ip)
{
	int64_t addr = page2addr(getIndexMap(oft_index));
	FILE *fp = fopen(FS_NAME, "rb+");
	fseek(fp, addr, SEEK_SET);
	fwrite(ip, sizeof(index_page), 1, fp);
	fclose(fp);
}

/* read index page content */
void read_indexPage(int32_t oft_index, index_page *ip)
{
	FILE *fp = fopen(FS_NAME, "rb");
	fseek(fp, page2addr(getIndexMap(oft_index)), SEEK_SET);
	fread(ip, sizeof(index_page), 1, fp);
	fclose(fp);

	/* DEBUG */
	printf("index page => number: %d, remain: %d\n", ip->page_number, ip->remain);
	for(vector<int32_t>::iterator it=ip->page_list.begin(); it!=ip->page_list.end(); it++)
		cout << *it << " ";
	cout << endl;

}

/*
delete the file from fs and return 0 if success, else return -1
*/
int myfs_file_delete(const char* filename)
{
	
}

/* 
return a page index in the fs 
if no place => return -1
*/
int32_t getFreePage()
{
	for(int i=0; i<header_info.fpt_entry_cnt; i++)
	{
		if(fpt_ptr[i].flag==0)
		{
			//printf("choose %d\n", i);
			FILE* fp = fopen(FS_NAME, "rb+"); // write back to fs
			fseek(fp, header_info.fpt_startadd+i, SEEK_SET);
			int one = 1;
			fwrite(&one, sizeof(char), 1, fp);
			fclose(fp);
			update_info();
			return i;
		}
	}
	return -1; // no empty page
}

/*
convert page index to addr offset
*/
int64_t page2addr(int32_t index)
{
	if(index<0)
		return -1;
	return (int64_t)(index*PAGE_SIZE+header_info.fpage_startadd);
}

/*
Init file system
call in every myfs function
*/
void init_fs()
{
	if(!strlen(FS_NAME))	// set FS_NAME by default
		select_fs(NULL);

	read_header();

	/* free last alloc if exist */
	if(oft_ptr)
		free(oft_ptr);
	if(fpt_ptr)
		free(fpt_ptr);

	/* alloc mem to oft and fpt handle ptr */
	oft_len = PAGE_SIZE*header_info.oft_pages;
	fpt_len = PAGE_SIZE*header_info.fpt_pages;
	oft_ptr = (OFT*)malloc(oft_len);
	fpt_ptr = (FPT*)malloc(fpt_len);

	update_info();

}

/*
read header and update tables
*/
void update_info()
{
	read_header();
	FILE *fp = fopen(FS_NAME, "rb");
	/* read table from fs file */
	fseek(fp, header_info.oft_startadd, SEEK_SET);
	fread(oft_ptr, sizeof(OFT), header_info.oft_entry_cnt, fp);
	fseek(fp, header_info.fpt_startadd, SEEK_SET);
	fread(fpt_ptr, sizeof(FPT), header_info.fpt_entry_cnt, fp);
	fclose(fp);
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
		fwrite(&header, sizeof(FS_HEADER), 1, fp); // write header to fs file

		/* all pages are free for init */
		fseek(fp, header.fpt_startadd, SEEK_SET);
		int8_t zero = 0;
		for(int i=0; i<header.fpt_entry_cnt; i++)
			fwrite(&zero, sizeof(zero), 1, fp);

		/* write end of table for oft and fpt */
		fclose(fp);
		init_fs(); 
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
	header->oft_startadd = sizeof(FS_HEADER);
	header->oft_entry_cnt = 0;
}

int init_FPT(int max_size, FS_HEADER* header)
{
	int32_t FPT_pages = (float)max_size/16.5/MB+1;
	header->fpt_pages = FPT_pages;
	header->fpt_startadd = sizeof(FS_HEADER)+header->oft_pages*PAGE_SIZE;
	header->fpt_entry_cnt = (float)max_size/MB*248;
	header->fpage_startadd = header->fpt_startadd+header->fpt_pages*PAGE_SIZE;
	//printf("%ld %ld\n", header->fpt_startadd, header->fpage_startadd);
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
		//printf("%d %d %ld %ld\n", header_info.oft_pages, header_info.fpt_pages, header_info.oft_startadd, header_info.fpt_startadd);
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
Set FS_NAME
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
		return 0;
	}
	else
		return -1;
}
