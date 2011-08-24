#ifndef CHEAP_TUNEZ_H_
#define CHEAP_TUNEZ_H_

#define MIDI_EVENTS_BUFFER_SIZE 4 // In bytes
#define MIDI_CHANNEL 0 // -> our synthesizer will answer statically to MIDI Channel 1

typedef struct _midi_byte_buffer {
	midi_byte data[MIDI_EVENTS_BUFFER_SIZE];
	uint8_t fill;
	uint8_t read_idx;
	uint8_t write_idx;
} midi_byte_buffer;

void sighandler(int signum);
void stack_value(midi_byte value);
midi_byte unstack_value(void);
void buffer_init(void);
void end(void);

#endif /* CHEAP_TUNEZ_H_ */
