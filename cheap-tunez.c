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
#include "cheap_midi.h"
#include "cheap_shm.h"
#include "cheap-tunez.h"

volatile midi_byte_buffer byte_buffer;
shm_struct *shared_segment = NULL;
int must_exit = 0;
int shmid = 0;

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
				printf("Stacking value... 0x%x\n",shared_segment->data.undecoded);
				stack_value(shared_segment->data);
				shared_segment->buffer_ready = 1;
			}
			break;
	}
}

// Ring buffer stack function
void stack_value(midi_byte value){
	printf("Calling stack_value(0x%x)\n",value.undecoded);
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
	printf("Calling unstack_value()\n");
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
	printf("Calling buffer_init()\n");
	byte_buffer.free = MIDI_EVENTS_BUFFER_SIZE - 1;
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

	shared_segment->buffer_ready = 1;

	while(must_exit == 0){
		if(byte_buffer.free == 0){
			printf("Unstacking values...\n");
			midi_byte mb;
			while (byte_buffer.free < MIDI_EVENTS_BUFFER_SIZE) {
				mb = unstack_value();
				printf("Value : 0x%x (%c)\n",mb.undecoded, mb.undecoded);
			}
			printf("End of stack\n");
		}
		pause();
	}
	return(0);
}
