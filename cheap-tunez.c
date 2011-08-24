#include <sys/ipc.h>
#include <sys/shm.h>
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
#include "cheap_midi.h"
#include "cheap_shm.h"
#include "cheap-tunez.h"

volatile midi_byte_buffer byte_buffer;
shm_struct *shared_segment = NULL;
int must_exit = 0;
int shmid = 0;

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
