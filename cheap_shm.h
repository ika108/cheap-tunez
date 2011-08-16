#ifndef CHEAP_SHM_H_
#define CHEAP_SHM_H_

// SHM Token ID (should be the same on the server code)
#define SHM_KEY 0xDEADBEEF

typedef struct _shm_struct {
        midi_byte data;
        uint8_t buffer_ready;
} shm_struct;


#endif
