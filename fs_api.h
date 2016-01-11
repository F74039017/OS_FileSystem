/***
File System Structure

***********************************************************
*   Header    *   OFT   *   FPL   *       Pages....       *
***********************************************************

OFT: Open file table
FPL: Free page list

Indexed Alloc => A page hold all pages which file need

4K page size => hold 36 oft entries
1M => need 7 pages for OFT
1 pages for FPT => hold 16.5M
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
#define PAGE_SIZE (4*KB)

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

typedef struct index_content {
	int32_t page_number;
	int32_t remain;
	vector<int32_t> page_list;
}index_page;

int select_fs(const char *filesystemname);
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
void index_page_wb(int32_t oft_index, index_page *ip);
int32_t allocNewPage(int32_t index);
void read_indexPage(int32_t index, index_page *ip);
void showEntries();
void showPPT();

int myfs_create(const char *filesystemname, int max_size);
int myfs_destroy(const char *filesystemname);
int myfs_file_delete(const char* filename);
int myfs_file_write(int fd, char *buf, int count);
int myfs_file_read(int fd, char *buf, int count);
int myfs_file_close(int fd);
int myfs_file_open(const char* filename);
int myfs_file_create(const char* filename);
