#include <sys/shm.h>
#include "cheap_shm.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

// Create a shared segment and return its pointer.
void *create_ssegment(void) {
	extern int shmid;
	extern shm_struct *shared_segment;

	shmid = shmget(SHM_KEY, sizeof(shm_struct), 0600); // First, try to get the shmid of an existing shm.
	if(shmid == -1 && errno == ENOENT) { // It seems there aren't any
		printf("Creating the shm segment...\n");
		shmid = shmget(SHM_KEY, sizeof(shm_struct), IPC_CREAT | 0600); // So let's create it. It's possible we can optimize the acl
		if(shmid == -1) { // Can't create a shm. bouh!
			fprintf(stderr, "Cannot create the shm segment: %i (%s)\n", errno, strerror(errno));
			return NULL;
		}
	}
	if(shmid == -1) {
		fprintf(stderr, "Cannot create the shm segment: %i (%s)\n", errno, strerror(errno));
		return NULL;
	}

	shared_segment = shmat(shmid, NULL, 0);
	if (shared_segment==(void*)-1) {
		fprintf(stderr, "Cannot attach to the shm segment: %i (%s)\n", errno, strerror(errno));
		return NULL;
	}
	return shared_segment;
}


