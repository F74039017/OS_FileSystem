#include "fs_api.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

int main()
{
puts("Create filesystem 1MB filesystem => myfs_create()");
	myfs_create("my_fs", 1*MB);

puts("Create file test file => myfs_file_create()");
	int create_fd = myfs_file_create("test"); // open the duplicate check after
	if(create_fd==-1)
	{
		puts("create file error");
		exit(EXIT_FAILURE);
	}
	else
		myfs_file_close(create_fd);
	showPPT();
	showEntries(5);
	puts("");
	
puts("Open file => myfs_file_open()");
	int fd = myfs_file_open("test");
	showPPT();
	puts("");

puts("Read dog picture and write to test file => myfs_file_write()");
	FILE *fp = fopen("dog.jpg", "rb");
	char buf[100*KB];
	int filesize = fread(buf, sizeof(char), 100*KB, fp);
	fclose(fp);
	fp = NULL;

	int ret = myfs_file_write(fd, buf, filesize);
	printf("Write = %d bytes\n", ret);

puts("Read test file and write to cp_pic.jpg => myfs_file_read()");
	char buf2[100*KB];
	int ret2 = myfs_file_read(fd, buf2, filesize);
	printf("Read = %d bytes\n", ret2);

	fp = fopen("cp_pic.jpg", "wb+");
	fwrite(buf2, sizeof(char), filesize, fp);
	fclose(fp);
	fp = NULL;
	puts("");

puts("Close test file => myfs_file_close()");
	myfs_file_close(fd);
	showPPT();
	puts("");

puts("Create file1 and file2");
	myfs_file_create("file1"); 
	myfs_file_create("file2");
	puts("");

puts("Try to create file1 again");
	if(myfs_file_create("file1")==-1)
		puts("  |=>The file already exist");
	puts("");

puts("Delete file1 => myfs_file_delete()");
	myfs_file_delete("file1");
	puts("");
	showEntries(5);

puts("myfs_destroy()");
puts("Do you want to delete the filesystem? y/n");
	char ch;
	ch = getchar();
	if(ch=='y')
		myfs_destroy("my_fs");

	return 0;
}
