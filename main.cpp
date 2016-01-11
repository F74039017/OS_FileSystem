#include "fs_api.h"
#include <cstdio>
#include <cstring>

int main()
{
	myfs_create("my_fs", 1*MB);
	myfs_file_create("snow_planet"); // open the duplicate check after
	
	int fd = myfs_file_open("snow_planet");

	FILE *fp = fopen("snow_planet.jpg", "rb");
	char buf[100*KB];
	int filesize = fread(buf, sizeof(char), 100*KB, fp);
	fclose(fp);
	fp = NULL;

	int ret = myfs_file_write(fd, buf, filesize);
	printf("write ret = %d\n", ret);
	char buf2[100];
	int ret2 = myfs_file_read(fd, buf2, filesize);
	printf("read ret = %d\n", ret2);
	myfs_file_close(fd);
	//showPPT();

	fp = fopen("cp_pic.jpg", "wb+");
	fwrite(buf2, sizeof(char), filesize, fp);
	fclose(fp);
	fp = NULL;

	//myfs_file_delete("Check");
	//showEntries();
	return 0;
}
