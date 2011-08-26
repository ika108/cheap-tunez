#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include "libs/cheap_midi.h"
#include "libs/cheap_shm.h"
#include "main.h"

volatile midi_byte_buffer byte_buffer;
int must_exit = 0;

static pthread_mutex_t mutex_stack = PTHREAD_MUTEX_INITIALIZER;

void sighandler(int signum) {
	//printf("signal %i received\n", signum);
	switch(signum) {
		case SIGINT: // Got Crtl+c
			if(must_exit == 1){ // We are forcing it
				end();
			} else {
				must_exit = 1; // Ask the main loop to end nicely
			}
			break;
		case SIGUSR1: // UART INTERRUPT
			if(shared_segment) {
				int ret;
//				printf("locking....\n");
				ret=pthread_mutex_lock(&mutex_stack);
				if(ret) {
					fprintf(stderr, "pthread_mutex_lock(): %s (%i)\n", strerror(ret), ret);
					break;
				}
				stack_value(shared_segment->data);
				ret=pthread_mutex_unlock(&mutex_stack);
				if(ret) fprintf(stderr, "pthread_mutex_unlock(): %s (%i)\n", strerror(ret), ret);
//				printf("unlocked....\n");
				ret=pthread_mutex_unlock(&shared_segment->buffer_ready);
				if(ret) fprintf(stderr, "pthread_mutex_lock(): %s (%i)\n", strerror(ret), ret);
				//fprintf(stderr,"Buffer debug : [wr_idx,read_idx,free,max] [%i,%i,%i,%i]\n", byte_buffer.write_idx,byte_buffer.read_idx,byte_buffer.free,MIDI_EVENTS_BUFFER_SIZE);
			}
			break;
	}
}

// Ring buffer stack function
void stack_value(midi_byte value){
	// printf("Calling stack_value(0x%x)\n",value.undecoded);
	printf("<==   STACK 0x%x\n", value.undecoded);
	if(byte_buffer.fill == MIDI_EVENTS_BUFFER_SIZE-1) {
		printf("Buffer overrun. Discarding : 0x%x\n",value.undecoded);
	} else {
		byte_buffer.data[byte_buffer.write_idx] = value;
		if(byte_buffer.write_idx == MIDI_EVENTS_BUFFER_SIZE-1) {
			byte_buffer.write_idx = 0;
		}
		else {
			byte_buffer.write_idx++;
		}
		byte_buffer.fill++;
	}
	//fprintf(stderr,"Buffer debug : [wr_idx,read_idx,free,max] [%i,%i,%i,%i]\n", byte_buffer.write_idx,byte_buffer.read_idx,byte_buffer.free,MIDI_EVENTS_BUFFER_SIZE);

	return;
}

// Ring buffer unstack function
midi_byte unstack_value(void) {
	//printf("Calling unstack_value()\n");
	midi_byte value;
	value.undecoded = 0x00;
	if(byte_buffer.fill>0) {
		value = byte_buffer.data[byte_buffer.read_idx];
		printf("==> UNSTACK 0x%x\n", value.undecoded);
		if( byte_buffer.read_idx == MIDI_EVENTS_BUFFER_SIZE-1) {
			byte_buffer.read_idx = 0;
		} else {
			byte_buffer.read_idx++;
		}
		byte_buffer.fill--;
	} else {
		printf("Buffer underrun.\n");
		//fprintf(stderr,"Buffer debug : [wr_idx,read_idx,free,max] [%i,%i,%i,%i]\n", byte_buffer.write_idx,byte_buffer.read_idx,byte_buffer.free,MIDI_EVENTS_BUFFER_SIZE);

		return(value);
	}
	//fprintf(stderr,"Buffer debug : [wr_idx,read_idx,free,max] [%i,%i,%i,%i]\n", byte_buffer.write_idx,byte_buffer.read_idx,byte_buffer.free,MIDI_EVENTS_BUFFER_SIZE);

	return(value);
}

void buffer_init(void){
	printf("Calling buffer_init()\n");
	byte_buffer.fill = 0;
	byte_buffer.read_idx = 0;
	byte_buffer.write_idx = 0;
	return;
}

void end(void) {
	int ret;
	if(shmid) {
		struct shmid_ds ds; // Just for the sake of it.
		ret = shmctl(shmid, IPC_RMID, &ds); // shm identifier, command, data_store
		if(ret == -1) { // Something's gone wrong...
			fprintf(stderr, "Cannot destroy shm key: %i (%s)", errno, strerror(errno));
		}
	}
	printf("Exiting...\n");
	exit(0);
}


int main(void) {
	struct sigaction signal_action;
	buffer_init();
	int ret;

	// Hookup the signal handler
	signal_action.sa_flags = SA_NODEFER;
	sigemptyset(&signal_action.sa_mask);
	signal_action.sa_handler = sighandler;
	if(sigaction(SIGINT, &signal_action, NULL) == -1) {
		fprintf(stderr, "Cannot set sighandler\n");
		end();
	}
	if(sigaction(SIGUSR1, &signal_action, NULL) == -1) {
		fprintf(stderr, "Cannot set sighandler\n");
		end();
	}

	// Connecting the shared segment
	shared_segment = create_ssegment();
	if(shared_segment == NULL) {
		fprintf(stderr, "Cannot create shared segment: %i (%s)\n", errno, strerror(errno));
		end();
	}

	pthread_mutex_init(&shared_segment->buffer_ready, NULL);

	midi_byte mb;
	while(must_exit == 0) {
		if(byte_buffer.fill>0) {
			//printf("Unstacking values...\n");
			//fprintf(stderr,"Buffer debug : [wr_idx,read_idx,free,max] [%i,%i,%i,%i]\n", byte_buffer.write_idx,byte_buffer.read_idx,byte_buffer.free,MIDI_EVENTS_BUFFER_SIZE);

			//int i; printf("buffer: "); for(i=0;i<MIDI_EVENTS_BUFFER_SIZE;i++) { printf("0x%x ", byte_buffer.data[i].undecoded); } printf("\n");
			ret=pthread_mutex_lock(&mutex_stack);
			if(ret) fprintf(stderr, "pthread_mutex_trylock(): %s (%i)\n", strerror(ret), ret);
			if(ret==EBUSY) continue;
			mb = unstack_value();
			ret=pthread_mutex_unlock(&mutex_stack);
			if(ret) fprintf(stderr, "pthread_mutex_unlock(): %s (%i)\n", strerror(ret), ret);
			//fprintf(stdout,"Buffer debug : [wr_idx,read_idx,fill,max] [%i,%i,%i,%i]\n", byte_buffer.write_idx,byte_buffer.read_idx,byte_buffer.fill,MIDI_EVENTS_BUFFER_SIZE);

			//printf("End of stack\n");
		}
		pause();
	}
	return(0);
}


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
#include "cheap_utils.h"
#include "cheap_midi.h"
#include "cheap_shm.h"
#include "midi_input.h"

// The segfaults seem to be related to the shm which is uninitialized. cf valgrind

/*  TODO : 
    Implement a correct error handler...
Sven : Read "select" man pages...
[DONE] Implement a way to gather the main synth process pid.
 */

int must_exit = 0; // Should we exit the code after catching a SIGINT ?
int midi_input_fd = 0; // Filedescriptor connected to our raw midi device
int shmid = 0; // Variable holding a shm identifier
pid_t serverpid = 0; // PID of the server process connected to the shm segment
shm_struct *shared_segment = NULL;

// This "callback" function is a hook for the linux kernel to pass a signal to our program.
static void sighandler(int signum) {
	printf("signal %i received\n", signum);
	switch(signum) {
		case SIGINT: // Got Crtl+c
			if(must_exit == 1){ // We are forcing it
				end();
			} else {
				must_exit = 1; // Ask the main loop to end nicely
			}
			break;
	}
}

// This is the end, my only friend, this end...
void end(void) {
	int ret;
	// If the midi device file decriptor is still open, then close it.
	if(midi_input_fd) {
		close(midi_input_fd);
	}
	// If the shm is still attached, then discard it.
	if(shmid) {
		struct shmid_ds ds; // Just for the sake of it.
		ret = shmctl(shmid, IPC_RMID, &ds); // shm identifier, command, data_store
		if(ret == -1) { // Something's gone wrong...
			fprintf(stderr, "Cannot destroy shm key: %i (%s)\n", errno, strerror(errno));
		}
	}
	printf("Exiting...\n");
	exit(0);
}

int main(int argc, char **argv) {
	int nbread;
	int ret;
	fd_set rfds;
	midi_byte buf;
	struct timeval tv;
	char *midi_input_device;
	struct sigaction signal_action; // Toothpaste :p
	int nbtries;

	// Parsing argv/argc
	if(argc != 2) {
		fprintf(stderr, "Usage: %s <device>\n", argv[0]);
		end();
	}

	// Opening a file descriptor to the midi raw device
	midi_input_device = argv[1];
	midi_input_fd = open(midi_input_device, O_RDONLY);
	if(midi_input_fd == -1) {
		fprintf(stderr, "Cannot open midi device %s: %i (%s)\n", midi_input_device, errno, strerror(errno));
		end();
	}

	// Hookup the signal handler
	signal_action.sa_flags = SA_NODEFER;
	sigemptyset(&signal_action.sa_mask);
	signal_action.sa_handler = sighandler;
	if(sigaction(SIGINT, &signal_action, NULL) == -1) {
		fprintf(stderr, "Cannot set sighandler\n");
		end();
	}

	// Connecting the shared segment
	shared_segment = create_ssegment();
	if(shared_segment == NULL) {
		fprintf(stderr, "Cannot create shared segment: %i (%s)\n", errno, strerror(errno));
		end();
	}

	while(must_exit == 0) {
		// Configure select
		FD_ZERO(&rfds);
		FD_SET(midi_input_fd, &rfds);
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		ret = select(midi_input_fd+1, &rfds, NULL, NULL, &tv);
		if(ret == -1) {
			if(errno == EINTR) {
				// Do not print anything, this is normal
				// No value was read in midi_input_fd. But that's the normal select behaviour
			} else {
				// Something bad happend while select'ing on the device.
				fprintf(stderr, "Select error %i (%s)\n", errno, strerror(errno));
			}
			break;
		} else if(ret == 0) {
			continue; // timeout
		}

		nbread = read(midi_input_fd, &buf, sizeof(midi_byte));
		if(nbread == -1) {
			fprintf(stderr, "Cannot read from midi device: %i (%s)\n", errno, strerror(errno));
			break;
		}
		else if(nbread == 0) {
			fprintf(stderr, "Cannot read from midi device: read 0 bytes\n");
			break;
		}
		else if(nbread != sizeof(midi_byte)) {
			fprintf(stderr, "read error. Wanted %zu bytes, got %i\n", sizeof(midi_byte), nbread);
			break;
		}

		if (serverpid == 0) {
			serverpid = find_pid_by_name("cheap-tunez");
			if(serverpid == 0) {
				fprintf(stderr, "Cannot find the midi server PID\n");
				break;
			}
		}
		printf("read %i byte : 0x%x\n", nbread, buf.undecoded);
		
		for(nbtries=0; nbtries<16; nbtries++) {
			ret=pthread_mutex_trylock(&shared_segment->buffer_ready);
			if(ret == EBUSY) {
				usleep(50);
			}
			else {
				//mutex locked
				memcpy(&(shared_segment->data),&buf,sizeof(midi_byte));
				kill(serverpid, SIGUSR1);
				//printf("KILL %i\n", serverpid);
			}
		}
		fprintf(stderr, "Buffer not ready, dropping value\n"); 
		// Maybe we can found a cleaner way to do this. Keeping it warm until next data arrives
		continue;
	}
	end();
	return 0;
}

