#ifndef KERNIX_BACKEND_H
#define KERNIX_BACKEND_H
#include "library.h"
#include <pthread.h>
#include <sys/types.h>
typedef enum { AUDIO_SELECT,AUDIO_PAUSE,AUDIO_NEXT,AUDIO_PREVIOUS,AUDIO_REPEAT,AUDIO_SHUFFLE,AUDIO_BLUETOOTH,AUDIO_AUX,AUDIO_VOLUME_UP,AUDIO_VOLUME_DOWN,AUDIO_PAIR,AUDIO_PAIR_STATUS } AudioAction;
typedef struct { AudioAction action;int playlist,song,value; } AudioCommand;
typedef struct {
 int playlist,song,playing,paused,repeat,shuffle,bluetooth,switching,mode_set,volume,pairing,paired,pair_checked;
 double elapsed,duration;char message[160];unsigned revision;
} AudioState;
typedef struct {
 pthread_t thread;pthread_mutex_t lock;int initialized,stop;
 AudioCommand commands[32];unsigned head,count;
 AudioState published,working;
 const MusicLibrary *library;
 pid_t decoder,router,control;int input,output;char incoming[8192];size_t used;
 int want_play,armed,decode_error,frame,resume_frame,route_bluetooth,route_ready;
 long route_deadline,control_deadline,next_pair_check;int control_pair;
 int *bag,bag_count;size_t bag_capacity;unsigned random;
} AudioBackend;
int audio_start(AudioBackend *b,const MusicLibrary *library);
int audio_send(AudioBackend *b,AudioCommand command);
void audio_snapshot(AudioBackend *b,AudioState *state);
void audio_shutdown(AudioBackend *b);
#endif
