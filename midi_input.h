#ifndef MIDI_INPUT_H_
#define MIDI_INPUT_H_

static void sighandler(int signum);
void end(void);
shm_struct *create_ssegment(void);

#endif
