#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <alsa/asoundlib.h>

static struct option options[] =
  {{"help", 0, 0, 'h'},
  {"alsa", 1, 0, 'm'},
  {"program", 1, 0, 'p'},
  {"control", 1, 0, 'c'},
  {"bend", 1, 0, 'b'},
  {"note", 1, 0, 'n'},
  {"off", 1, 0, 'o'},
  {0, 0, 0, 0}};
  
int main(int argc, char *argv[]) {

  int getopt_return;
  int option_index;
  int valid;
  snd_seq_t *seq_handle;
  int out_port;
  snd_seq_addr_t seq_addr;
  snd_seq_event_t ev;
  
  valid = 0;
  while (((getopt_return = getopt_long(argc, argv, "ha:p:c:b:n:o:", options, &option_index)) >= 0) && (valid < 2)) {
    switch(getopt_return) {
      case 'a':
        valid++;
        if (snd_seq_open(&seq_handle, "hw", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
          fprintf(stderr, "Error opening ALSA sequencer.\n");
          exit(1);  
        }
        snd_seq_set_client_name(seq_handle, "MidiSend");
        if ((out_port = snd_seq_create_simple_port(seq_handle, "MidiSend",
                        SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
                        SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
          fprintf(stderr, "Error creating sequencer port.\n");
          exit(1);
        }   
        snd_seq_parse_address(seq_handle, &seq_addr, optarg);
        snd_seq_connect_to(seq_handle, out_port, seq_addr.client, seq_addr.port);
        snd_seq_ev_clear(&ev);
        snd_seq_ev_set_subs(&ev);  
        snd_seq_ev_set_direct(&ev);
        snd_seq_ev_set_source(&ev, out_port);
        break;
      case 'p':
        if (valid) {
          valid++;
          if (argc < 6) {
            fprintf(stderr, "\nMissing parameter(s): midisend --program <ch> <prg>\n\n");
          } else {
            ev.data.control.channel = atoi(optarg);
            ev.type = SND_SEQ_EVENT_PGMCHANGE;
            ev.data.control.value = atoi(argv[optind]);
          }
        }  
        break;
      case 'c':
        if (valid) {
          valid++;
          if (argc < 7) {
            fprintf(stderr, "\nMissing parameter(s): midisend --control <ch> <ctrl> <val>\n\n");
          } else {
            ev.data.control.channel = atoi(optarg);
            ev.type = SND_SEQ_EVENT_CONTROLLER;
            ev.data.control.param = atoi(argv[optind]);
            ev.data.control.value = atoi(argv[optind + 1]);
          }   
        }  
        break;
      case 'b':
        if (valid) {
          valid++;
          if (argc < 6) {
            fprintf(stderr, "\nMissing parameter(s): midisend --bend <ch> <val>\n\n");
          } else {
            ev.data.control.channel = atoi(optarg);
            ev.type = SND_SEQ_EVENT_PITCHBEND;
            ev.data.control.value = atoi(argv[optind]) - 8192;
          }
        }  
        break;
      case 'n':
        if (valid) {
          valid++;
          if (argc < 8) {
            fprintf(stderr, "\nMissing parameter(s): midisend --note <ch> <on> <num> <vel>\n\n");
          } else {
            ev.data.control.channel = atoi(optarg);
            ev.type = (atoi(argv[optind])) ? SND_SEQ_EVENT_NOTEON : SND_SEQ_EVENT_NOTEOFF;
            ev.data.note.note = atoi(argv[optind + 1]);
            ev.data.note.velocity = atoi(argv[optind + 2]);
          }  
        }  
        break;
      case 'o':
        if (valid) {
          valid++;
          if (argc < 5) {
            fprintf(stderr, "\nMissing parameter(s): midisend --off <ch>\n\n");
          } else {
            ev.data.control.channel = atoi(optarg);
            ev.type = SND_SEQ_EVENT_CONTROLLER;
            ev.data.control.param = MIDI_CTL_ALL_NOTES_OFF;
            ev.data.control.value = 0;
          }   
        }  
        break;
      case 'h':
        valid = 3;
        printf("\nMidiSend 0.0.1\nWritten by Matthias Nagorni\n");
        printf("(c)2004 SUSE Linux AG Nuremberg\n");
        printf("Licensed under GNU General Public License\n\n");
        printf("--alsa <port>                  ALSA Sequencer Port (e.g. 64:0)\n");
        printf("--program <ch> <prg>           Program Change\n");
        printf("--control <ch> <ctrl> <val>    Control Change\n");
        printf("--bend <ch> <val>              Pitch Bend (0..16383, center = 8192)\n");
        printf("--note <ch> <on> <num> <vel>   Note On (on=1) Note Off (on=0)\n");
        printf("--off <ch>                     All Notes Off\n\n");
        exit(EXIT_SUCCESS);
    }
  }
  if (valid < 2) {
    fprintf(stderr, "\nUsage: midisend PORT COMMAND PARAMETERS\n\n");
    fprintf(stderr, "Type \'midisend -h\' for a list of valid commands.\n\n");
  } else if (valid == 2) {
    snd_seq_event_output_direct(seq_handle, &ev);
    snd_seq_close(seq_handle);
  }
}
