#ifndef CHEAP_SHM_H_
#define CHEAP_SHM_H_

// SHM Token ID (should be the same on the server code)
#define SHM_KEY 0xDEADBEEF

#include <stdint.h>
#include "cheap_midi.h"

typedef struct _shm_struct {
        midi_byte data;
        uint8_t buffer_ready;
} shm_struct;

void *create_ssegment(void);

#endif
