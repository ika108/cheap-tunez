#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>


#define SHM_KEY 0xDEEEBEEF

int must_exit=0;
int midi_input_fd=0;
int shmid=0;

typedef struct _shared_struct {
		unsigned char byte;
		unsigned char buffer_ready;	
} shared_struct;

void sighandler(int signum) {
	printf("signal %i received\n", signum);
	switch(signum) {
		case SIGINT:
			must_exit=1;
		break;
	}
}

void end(void) {
		int ret;
		if(midi_input_fd) {
				close(midi_input_fd);
		}
		if(shmid) {
				struct shmid_ds ds;
				ret=shmctl(shmid, IPC_RMID, &ds);
				if(ret==-1) {
						fprintf(stderr, "Cannot destroy shm key: %i (%s)", errno, strerror(errno));
				}
		}
		printf("Exiting...\n");
		exit(0);
}

int create_shm_key(void) {
    shmid = shmget(SHM_KEY, sizeof(shared_struct), 0666);
	if(shmid==-1 && errno==ENOENT) {
			shmid = shmget(SHM_KEY, sizeof(shared_struct), IPC_CREAT | 0666);
			if(shmid==-1) {
					fprintf(stderr, "Cannot create shm key: %i (%s)", errno, strerror(errno));
					return 0;
			}
	}
	return shmid;
}

int main(int argc, char **argv) {
		unsigned char midi_input_bytes[1];
		int nbread;
		int ret;
		if(argc!=2) {
				fprintf(stderr, "Usage: %s <device>\n", argv[0]);
				end();
		}
		char *midi_input_device=argv[1];
		midi_input_fd=open(midi_input_device, O_RDONLY);
		if(midi_input_fd==-1) {
				fprintf(stderr, "Cannot open midi device %s: %i (%s)\n", midi_input_device, errno, strerror(errno));
				end();
		}

		fd_set rfds;
		struct timeval tv;
		

		/* TODO: get the return value as sighandler_t */
		if(signal(SIGINT, sighandler)==SIG_ERR) {
				fprintf(stderr, "Cannot set sighandler\n");
				end();
		}

		shmid=create_shm_key();
		if(shmid==0) {
			end();
		}

		while(must_exit==0) {
				FD_ZERO(&rfds);
				FD_SET(midi_input_fd, &rfds);
				tv.tv_sec = 1;
				tv.tv_usec = 0;
				ret = select(midi_input_fd+1, &rfds, NULL, NULL, &tv);
				if(ret==-1) {
						if(errno==EINTR) {
								//do not print anything, this is normal
						}
						else {
								fprintf(stderr, "Select error %i (%s)\n", errno, strerror(errno));
						}
						break;
				}
				else if(ret==0) {
						continue; // timeout
				}
				nbread=read(midi_input_fd, &midi_input_bytes, sizeof(midi_input_bytes));
				if(nbread==-1) {
						fprintf(stderr, "Cannot read from midi device: %i (%s)\n", errno, strerror(errno));
						break;
				}
				else if(nbread==0) {
						fprintf(stderr, "Cannot read from midi device: read 0 bytes\n");
						break;
				}
				printf("read %i bytes: %i 0x%x\n", nbread, midi_input_bytes[0], midi_input_bytes[0]);
		}

		end();

		return 0;
}

