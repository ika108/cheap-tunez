#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include "midi.h"

// SHM Token ID (should be the same on the client code)
#define SHM_KEY 0xDEADBEEF

volatile midi_byte_buffer byte_buffer;
midi_byte *shared_segment;
int must_exit = 0;
int shmid = 0;

void sighandler(int signum);
void stack_value(midi_byte value);
midi_byte unstack_value(void);
void buffer_init(void);
midi_byte *create_ssegment(void);
void end(void);

void sighandler(int signum) {
	printf("signal %i received\n", signum);
	switch(signum) {
		case SIGINT: // Got Crtl+c
			if(must_exit == 1){ // We are forcing it
				end();
			} else {
				must_exit = 1; // Ask the main loop to end nicely
			}
			break;
		case SIGUSR1: // UART INTERRUPT
			printf("Stacking value...\n");
			stack_value(*shared_segment);
			break;
	}
}

// Ring buffer stack function
void stack_value(midi_byte value){
	if(byte_buffer.free != 0){
		byte_buffer.data[byte_buffer.write_idx] = value;
		if( byte_buffer.write_idx == ( MIDI_EVENTS_BUFFER_SIZE - 1 ) ) {
			byte_buffer.write_idx = 0;
		} else {
			byte_buffer.write_idx++;
		}
		byte_buffer.free--;
	} else {
		printf("Buffer overrun. Discarding : 0x%x\n",value.undecoded);
	}
	return;
}

// Ring buffer unstack function
midi_byte unstack_value(void){
	midi_byte value;
	value.undecoded = 0x00;
	if(byte_buffer.free != MIDI_EVENTS_BUFFER_SIZE){
		value = byte_buffer.data[byte_buffer.read_idx];
		if( byte_buffer.read_idx == ( MIDI_EVENTS_BUFFER_SIZE - 1 ) ) {
			byte_buffer.read_idx = 0;
		} else {
			byte_buffer.read_idx++;
		}
		byte_buffer.free++;
	} else {
		printf("Buffer underrun.\n");
		return(value);
	}
	return(value);
}

void buffer_init(void){
	byte_buffer.free = MIDI_EVENTS_BUFFER_SIZE - 1;
	byte_buffer.read_idx = 0;
	byte_buffer.write_idx = 0;
	return;
}

midi_byte *create_ssegment(void) {
	midi_byte *ssegment;
	shmid = shmget(SHM_KEY, sizeof(char), 0600); // First, try to get the shmid of an existing shm.
	if(shmid == -1 && errno == ENOENT) { // It seems there aren't any
		shmid = shmget(SHM_KEY, sizeof(char), IPC_CREAT | 0600); // So let's create it. It's possible we can optimize the acl
		if(shmid == -1) { // Can't create a shm. bouh!
			return NULL;
		}
		ssegment = shmat(shmid, NULL, 0);
	}
	return ssegment;
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
		fprintf(stderr, "Cannot create shared segment: %i (%s)", errno, strerror(errno));
		end();
	}

	while(must_exit == 0){
		if(byte_buffer.free == 0){
			printf("Unstacking values...\n");
			midi_byte value;
			while( (value = unstack_value()).undecoded > 0x00 ){
				printf("Value : 0x%x (%c)\n",value.undecoded, value.undecoded);
			}
			printf("End of stack\n");
		}
		pause();
	}
	return(0);
}
