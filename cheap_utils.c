#include "stdio.h"
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "cheap_utils.h"

int find_pid_by_name(const char *name) {
	DIR *od=opendir("/proc");
	int fd=0;
	char fname[256*2];
	char buf[256*2]="\0";
	struct dirent *de;
	size_t nbread;
	int pid=-1;
	if(!od) return -1;
	while ((de = readdir(od)) != NULL)
	{
		snprintf(fname, sizeof(fname), "/proc/%s/cmdline", de->d_name);
		fd=open(fname, O_RDONLY);
		if(fd==-1) continue;
		nbread=read(fd, buf, sizeof(buf));
		if(nbread==-1)  {
			close(fd);
			continue;
		}
		if(strstr(buf, name)) {
			//now, check that the executable name ends with the name we search. This will prevent grabbing the PID of vi, fro instance, if we have a "vi midi_input.c"

			snprintf(fname, sizeof(fname), "/proc/%s/exe", de->d_name);
			nbread=readlink(fname, buf, sizeof(buf));
			if(nbread==-1) {
				close(fd);
				continue;
			}
			buf[nbread]='\0';
			if(strlen(buf)>strlen(name)) {
				if(strcmp(buf+strlen(buf)-strlen(name), name)==0) {
					pid=atoi(de->d_name);
					break;
				}

}
		}
	}

	if(fd) close(fd);
	if(od) closedir(od);
	return pid;
}

//rename the func to 'main' to test
int _main() {
#define NAME "midi_input"
	printf("pid for %s: %i\n", NAME, find_pid_by_name(NAME));
	return 0;
}
