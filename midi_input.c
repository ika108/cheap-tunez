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

/*  TODO : 
	Implement a correct error handler...
	Sven : Read "select" man pages...
	Implement a way to gather the main synth process pid.
*/

#define SPID 1234

// SHM Token ID (should be the same on the client code)
#define SHM_KEY 0xDEADBEEF

int must_exit = 0; // Should we exit the code after catching a SIGINT ?
int midi_input_fd = 0; // Filedescriptor connected to our raw midi device
int shmid = 0; // Variable holding a shm identifier
pid_t serverpid = 0; // PID of the server process connected to the shm segment


static void sighandler(int signum);
void end(void);
char *create_ssegment(void);

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
	// If the shm is still mounted, the discard it.
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

// Create a shared segment and return its pointer.
char *create_ssegment(void) {
	char *ssegment;
	shmid = shmget(SHM_KEY, sizeof(char), 0666); // First, try to get the shmid of an existing shm.
	if(shmid == -1 && errno == ENOENT) { // It seems there aren't any
		shmid = shmget(SHM_KEY, sizeof(char), IPC_CREAT | 0666); // So let's create it. It's possible we can optimize the acl
		if(shmid == -1) { // Can't create a shm. bouh!
			return NULL;
		}
		ssegment = shmat(shmid,NULL,0 );
	}
	return ssegment;
}

int main(int argc, char **argv) {
	int nbread;
	int ret;
	fd_set rfds;
	struct timeval tv;
	char *midi_input_device;
	char *shared_segment;
	struct sigaction signal_action; // Toothpast :p

	if(argc != 2) {
		fprintf(stderr, "Usage: %s <device>\n", argv[0]);
		end();
	}
	midi_input_device = argv[1];
	midi_input_fd = open(midi_input_device, O_RDONLY);
	if(midi_input_fd == -1) {
		fprintf(stderr, "Cannot open midi device %s: %i (%s)\n", midi_input_device, errno, strerror(errno));
		end();
	}

	// Hookup the signal handler
	signal_action.sa_flags = SA_NODEFER;
	sigemptyset(&signal_action.sa_mask);
	signal_action.sa_sigaction = sighandler;
	if(sigaction(SIGINT, &signal_action, NULL) == -1) {
		fprintf(stderr, "Cannot set sighandler\n");
                end();
	}
	
	// Connecting the shared segment
	shared_segment = create_ssegment();
	if(shared_segment == NULL) {
		fprintf(stderr, "Cannot create shared segment: %i (%s)", errno, strerror(errno));
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

		nbread = read(midi_input_fd, shared_segment, sizeof(char));
		if(nbread == -1) {
			fprintf(stderr, "Cannot read from midi device: %i (%s)\n", errno, strerror(errno));
			break;
		}
		else if(nbread == 0) {
			fprintf(stderr, "Cannot read from midi device: read 0 bytes\n");
			break;
		}
		if(serverpid == 0) {
			// TODO ; Must find a way to gather server pid... Via the shared segment perhaps but its data size is char.
			// and t_pid... I don't know. And it seems to be plateform dependent.
			serverpid = SPID;
		}
		printf("read %i byte : 0x%x\n", nbread, atoi(shared_segment));
		kill(serverpid, SIGUSR1);
	}
	end();
	return 0;
}

