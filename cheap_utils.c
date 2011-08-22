#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/shm.h>
#include "cheap_shm.h"
#include "cheap_utils.h"

int find_pid_by_name(const char *name) {
	DIR *od = opendir("/proc"); // Create a FD for /proc directory
	int fd = 0;
	char fname[256*2];
	char buf[256*2] = "\0";
	struct dirent *de;
	size_t nbread;
	int pid = 0;
	if(!od) return -1; // Couldn't open /proc
	while ((de = readdir(od)) != NULL) { // Iteration over /proc content
		/* This trick can work only because readdir iterate on an alphanumeric order. Making the pid numbers first.
		Maybe we should verify that de->d_name is a number before processing it */
		snprintf(fname, sizeof(fname), "/proc/%s/cmdline", de->d_name); // Store next cmdline content into fname
		fd = open(fname, O_RDONLY);
		if(fd == -1) continue; // If we fail to open fname, (and we will) just skip the entry.
		nbread = read(fd, buf, sizeof(buf)); // Try to read as much chars as possible
		if(nbread == -1) { // Couldn't read anything. Wierd file ? 
			close(fd);
			continue; // Skip the entry
		}
		if(strstr(buf, name)) {
			/* now, check that the executable name ends with the name we search. 
			   This will prevent grabbing the PID of vi, fro instance, if we have a "vi midi_input.c" */
			snprintf(fname, sizeof(fname), "/proc/%s/exe", de->d_name); // Store the exec proc link into fname
			nbread = readlink(fname, buf, sizeof(buf)); // Try to get the full path of the exec
			if(nbread == -1) { // Couldn't resolve the link
				close(fd);
				continue; // Skip the entry
			}
			buf[nbread] = '\0'; // Add a carriage return
			if(strlen(buf) > strlen(name)) { 
				if(strcmp(buf + strlen(buf) - strlen(name), name) == 0) { // Last check to verify if the exec file correspond to the queried name
					pid = atoi(de->d_name); // Store pid
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
