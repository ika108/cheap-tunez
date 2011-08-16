#ifndef CHEAP_MIDI_H_
#define CHEAP_MIDI_H_

// Be careful, all this values have been pre-aligned, so it won't match the midi specs values
// Channel voice messages (3 bits)
#define MIDI_NoteOff                    0x00
#define MIDI_NoteOn                     0x01
#define MIDI_PolykeyPressure            0x02
#define MIDI_ControlChange              0x03 // Also check Channel mode messages below
#define MIDI_ProgramChange              0x04
#define MIDI_ChannelPressure            0x05
#define MIDI_PitchBend                  0x06
#define MIDI_System                     0x07 // This is the representation of a system message in voice message mode.

// Channel mode messages (7 bits)
#define MIDI_Reset_Ctrl                 0x79
#define MIDI_Local_Ctrl                 0x7A
#define MIDI_All_Notes_off              0x7B
#define MIDI_Omni_Mode_off              0x7C
#define MIDI_Omni_Mode_on               0x7D
#define MIDI_Mono_Mode_on               0x7E
#define MIDI_Poly_Mode_on               0x7F

// System messages (7 bits)
#define MIDI_StartSysEx                 0x70
#define MIDI_Timing_Code                0x71
#define MIDI_Sng_Pos_Ptr                0x72
#define MIDI_Sng_select                 0x73
#define MIDI_Tune_request               0x76
#define MIDI_EndSysEx                   0x77
#define MIDI_TimingClock                0x78
#define MIDI_Start                      0x7A
#define MIDI_Continue                   0x7B
#define MIDI_Stop                       0x7C
#define MIDI_ActiveSensing              0x7E
#define MIDI_SystemReset                0x7F

typedef union _midi_byte {
        uint8_t undecoded;
        struct _part_midi_byte {
                uint8_t isstatus : 1;
                uint8_t data : 7;
        } part_midi_byte;
} midi_byte;

typedef union _midi_event {
        uint32_t undecoded;
        struct _voice_midi_event {
                uint8_t isstatus : 1;
                uint8_t event : 3;
                uint8_t channel : 4;
                midi_byte data[2];
                uint8_t tocompletion : 2;
                uint8_t : 6; // Padding for 32 bits alignment
        } voice_midi_event;
        struct _system_midi_event {
                uint8_t isstatus : 1;
                uint8_t event : 7;
                midi_byte data[2];
                uint8_t tocompletion : 2;
                uint8_t : 6; // Padding for 32 bits alignment
        } system_midi_event;
} midi_event;

#endif
