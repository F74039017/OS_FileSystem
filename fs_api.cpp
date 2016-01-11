#include "fs_api.h"

char FS_NAME[100]; // name of the selected file system
const char FS_EXT[4] = ".fs";
unsigned fd_cnt = 0; 
map<int32_t, int32_t> ppt;	// per-process table
FS_HEADER header_info; // read from the start of the fs file
OFT *oft_ptr = NULL; 
FPT *fpt_ptr = NULL;
size_t oft_len, fpt_len;


/* show ppt map emtry */
void showPPT()
{
	//cout << "Show PPT" << endl;
	for(map<int32_t, int32_t>::iterator it=ppt.begin(); it!=ppt.end(); it++)
		cout << it->first << " " << it->second << endl;
}

/* show all files in fs */
void showEntries()
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

/*
Free file used pages and modify oft table.
return 0 if success, else -1
*/
int myfs_file_delete(const char* filename)
{
	init_fs();

	FILE* fp = fopen(FS_NAME, "rb");
	fseek(fp, header_info.oft_startadd, SEEK_SET);
	OFT temp;
	for(int i=0; i<header_info.oft_entry_cnt; i++) // i => oft index
	{
		fread(&temp, sizeof(OFT), 1, fp);
		if(strcmp(temp.filename, filename)==0)
		{
			fclose(fp); // find the oft entry

			/* Free fpages */
			int32_t oft_index = i;
			//cout << "delete oft_index = " << oft_index << endl;
			index_page ip;
			read_indexPage(oft_index, &ip);
			FILE* ffp = fopen(FS_NAME, "rb+");
			int8_t zero = 0;
			for(vector<int32_t>::iterator it=ip.page_list.begin(); it!=ip.page_list.end(); it++)
			{	
				fseek(ffp, header_info.fpt_startadd+*it, SEEK_SET);
				fwrite(&zero, sizeof(zero), 1, ffp);
			}
			fclose(ffp);

			/* Free oft entry */
			strcpy(temp.filename, ""); // clean filename
			int64_t addr_offset = header_info.oft_startadd+i*sizeof(OFT); // calculate the addr of new entry
			FILE* efp = fopen(FS_NAME, "rb+");
			fseek(efp, addr_offset, SEEK_SET);
			fwrite(&temp, sizeof(OFT), 1, efp);	// overwrite
			fclose(efp);

			return 0;
		}
	}
	fclose(fp);
	return -1;
}

/*
write content to file
return how success number after write, if fd is invalid => return -1
*/
int myfs_file_write(int fd, char *buf, int count)
{
	int ret = 0;
	init_fs();
	
	int32_t oft_index = getPOMap(fd);
	if(oft_index<0)
		return -1;
	//cout << "oft_index = " << oft_index << endl;
	index_page ip;
	read_indexPage(oft_index, &ip);
	
	int rest = count-ip.remain;
	int need = 0;
	int c_count = count; 
	int extra_page = 0;
	if(rest>0) // need more pages
	{
		need = rest/PAGE_SIZE;
		c_count = rest%PAGE_SIZE;
		extra_page = 0;
		if(c_count>0)
			need++;
		else if(c_count==0)	// just fit
			extra_page = 1;
	}
	else if(rest==0)
		extra_page = 1;

	FILE* fp = fopen(FS_NAME, "rb+");
	
	/* write remain */
	int32_t last_page_index = ip.page_list.back();
		//printf("need page = %d, c_count = %d, extra_page = %d, last_page_index = %d\n", need, c_count, extra_page, last_page_index);
	int32_t page_offset = PAGE_SIZE-ip.remain;
		//cout << page_offset << " " << PAGE_SIZE << " " << ip.remain << endl;
	fseek(fp, page2addr(last_page_index)+page_offset, SEEK_SET);
	ret += fwrite(buf, sizeof(char), ip.remain, fp);

	/* write new page */
	for(int i=1; i<=need; i++)
	{
		int32_t newPage_index = allocNewPage(oft_index);
		fseek(fp, page2addr(newPage_index), SEEK_SET);
		if(i!=need)	// not last page => fill it
			ret += fwrite(buf+i*PAGE_SIZE, sizeof(char), PAGE_SIZE, fp);
		else
			ret += fwrite(buf+i*PAGE_SIZE, sizeof(char), c_count, fp);
	}
	read_indexPage(oft_index, &ip);
	ip.remain -= c_count;
	/* DEBUG */
	//printf("c_count = %d, ip remain = %d\n", c_count, ip.remain);

	index_page_wb(oft_index, &ip);

	/* extra page */
	if(extra_page)
		allocNewPage(oft_index);

	fclose(fp);
	return ret;
}

/*
read content to file
return how success number after write
If fd is invalid => return -1
If count is bigger than file size => retuen -1
*/
int myfs_file_read(int fd, char *buf, int count)
{
	int ret = 0;
	init_fs();
	
	int32_t oft_index = getPOMap(fd);
	if(oft_index<0)
		return -1;
	//cout << "oft_index = " << oft_index << endl;
	index_page ip;
	read_indexPage(oft_index, &ip);

	int need = count/PAGE_SIZE;
	int c_count = count%PAGE_SIZE;
	if(need>ip.page_list.size())	
		return -1;

	FILE* fp = fopen(FS_NAME, "rb");
	/* read whole pages */
	for(int i=0; i<need; i++)
	{
		fseek(fp, page2addr(ip.page_list[i]), SEEK_SET);
		ret += fread(buf+i*PAGE_SIZE, sizeof(char), PAGE_SIZE, fp);
	}
	/* read part of page */
	if(c_count>0)
	{
		fseek(fp, page2addr(ip.page_list[need]), SEEK_SET);
		ret += fread(buf+need*PAGE_SIZE, sizeof(char), c_count, fp);
	}
	return ret;
}

/* 
open a file in fs and return the file descriptor 
if the file doesn't exist => return -1
*/
int myfs_file_open(const char* filename)
{
	init_fs();

	FILE* fp = fopen(FS_NAME, "rb+");
	fseek(fp, header_info.oft_startadd, SEEK_SET);
	OFT temp;
	for(int i=0; i<header_info.oft_entry_cnt; i++)
	{
		fread(&temp, sizeof(OFT), 1, fp);
		if(strcmp(temp.filename, filename)==0)
		{
			ppt[++fd_cnt] = i;
			fclose(fp);
			//printf("No. %d map to index page %d, return desc = %d\n", i, temp.index, fd_cnt);
			return fd_cnt;
		}
	}
	fclose(fp);
	return -1;

	/* DEBUG */
	/*
	printf("check oft_startadd = %ld\n", header_info.oft_startadd);
	printf("create file with desc %d to %d\n", fd_cnt, ppt[fd_cnt]);
	printf("first entry is name: %s and page index: %d\n", oft_ptr[0].filename, oft_ptr[0].index);
	printf("header oft entry cnt: %d\n", header_info.oft_entry_cnt);
	*/
}

/*
close the file with desc
if the fd is invalid => return -1, else return 0
*/
int myfs_file_close(int fd)
{
	init_fs();
	
	int32_t oft_index = getPOMap(fd);
	if(oft_index<0)
		return -1;

	ppt.erase(fd);	
	return 0;
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
	//printf("get index map %d\n", oft_ptr[index].index);
	return oft_ptr[index].index;
}

/*
add new entry to OFT, return the index of free page
if retuen value is -1 => no more place
*/
int32_t oft_addEntry(const char* filename) 
{
	/* Find empty entries */
	FILE* fp = fopen(FS_NAME, "rb+");
	fseek(fp, header_info.oft_startadd, SEEK_SET);
	OFT temp;
	for(int i=0; i<header_info.oft_entry_cnt; i++)
	{
		fread(&temp, sizeof(OFT), 1, fp);
		if(strcmp(temp.filename, "")==0)
		{
			int64_t addr_offset = header_info.oft_startadd+i*sizeof(OFT); // calculate the addr of new entry
			OFT entry;
			entry.index = getFreePage();	// assign free page for indexed page
			strcpy(entry.filename, filename);
			FILE* efp = fopen(FS_NAME, "rb+"); // write new entry to fs file
			fseek(efp, addr_offset, SEEK_SET);
			//printf("temp entry => name: %s, index: %d\n", entry.filename, entry.index);
			fwrite(&entry, sizeof(OFT), 1, efp);
			update_info();
			fclose(efp);
			fclose(fp);
			return i; // return entry index of oft
		}
	}
	fclose(fp);
	return -1;
}

/* 
create a file in fs and return the file descriptor 
if the file name is duplicated => return -1
*/
int myfs_file_create(const char* filename)
{
	init_fs();
	/* check same name */
	FILE* fp = fopen(FS_NAME, "rb+");
	fseek(fp, header_info.oft_startadd, SEEK_SET);
	OFT temp;
	for(int i=0; i<header_info.oft_entry_cnt; i++)
	{
		fread(&temp, sizeof(OFT), 1, fp);
		if(strcmp(temp.filename, filename)==0)
		{
			fclose(fp);
			return -1;
		}
	}
	fclose(fp);


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

	/* DEBUG */
	/*
	printf("check oft_startadd = %ld\n", header_info.oft_startadd);
	printf("create file with desc %d to %d\n", fd_cnt, ppt[fd_cnt]);
	printf("index of ip = %d\n", index);
	//printf("first entry is name: %s and page index: %d\n", oft_ptr[2].filename, oft_ptr[2].index);
	printf("header oft entry cnt: %d\n", header_info.oft_entry_cnt);
	*/

	return fd_cnt; // pp desc
}

/*
alloc new page to specific oft index page
return new page index
*/
int32_t allocNewPage(int32_t oft_index)
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
	return index;
}

/* write index page data */
void index_page_wb(int32_t oft_index, index_page *ip)
{
	int64_t addr = page2addr(getIndexMap(oft_index));
	FILE *fp = fopen(FS_NAME, "rb+");
	fseek(fp, addr, SEEK_SET);
	int32_t tpage_number = ip->page_number;
	fwrite(&tpage_number, sizeof(int32_t), 1, fp);
	int32_t tremain = ip->remain;
	fwrite(&tremain, sizeof(int32_t), 1, fp);
	for(vector<int32_t>::iterator it=ip->page_list.begin(); it!=ip->page_list.end(); it++)
	{
		int32_t temp = *it;
		fwrite(&temp, sizeof(int32_t), 1, fp);
	}
	fclose(fp);
}

/* read index page content */
void read_indexPage(int32_t oft_index, index_page *ip)
{
	FILE *fp = fopen(FS_NAME, "rb");
	fseek(fp, page2addr(getIndexMap(oft_index)), SEEK_SET);
	int32_t tpage_number;
	fread(&tpage_number, sizeof(int32_t), 1, fp);
	ip->page_number = tpage_number;
	int32_t tremain;
	fread(&tremain, sizeof(int32_t), 1, fp);
	ip->remain = tremain;

	ip->page_list.clear();
	for(int i=0; i<tpage_number; i++)
	{
		int32_t temp;
		fread(&temp, sizeof(int32_t), 1, fp);
		ip->page_list.push_back(temp);
	}
	fclose(fp);
	/* DEBUG */
	/*
	printf("index page => number: %d, remain: %d\n", ip->page_number, ip->remain);
	for(vector<int32_t>::iterator it=ip->page_list.begin(); it!=ip->page_list.end(); it++)
		cout << *it << " ";
	cout << endl;
	*/
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

		/* init all oft entries */
		fseek(fp, header.oft_startadd, SEEK_SET);
		OFT temp;
		strcpy(temp.filename, "");
		temp.index = -1;
		for(int i=0; i<header.oft_entry_cnt; i++)
			fwrite(&temp, sizeof(OFT), 1, fp);

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
	header->oft_entry_cnt = OFT_pages*36;
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
