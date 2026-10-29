#define _POSIX_C_SOURCE 200809L
#include "backend.h"
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
extern char **environ;
static void publish(AudioBackend *b) {
 pthread_mutex_lock(&b->lock);b->working.revision++;b->published=b->working;pthread_mutex_unlock(&b->lock);
}
static void message(AudioBackend *b,const char *text) { snprintf(b->working.message,sizeof b->working.message,"%s",text); }
static const LibraryPlaylist *playlist(AudioBackend *b) {
 return b->working.playlist>=0 && (size_t)b->working.playlist<b->library->playlist_count ? &b->library->playlists[b->working.playlist] : NULL;
}
static const LibraryTrack *track(AudioBackend *b) {
 const LibraryPlaylist *p=playlist(b);
 return p && b->working.song>=0 && (size_t)b->working.song<p->count ? &b->library->tracks[p->first+(size_t)b->working.song] : NULL;
}
static const char *helper(void) { const char *p=getenv("KERNIX_AUDIO_HELPER");return p&&*p?p:"/usr/bin/kernix-output"; }
static void terminate(pid_t *pid) {
 if(*pid<=0) return;
 kill(-*pid,SIGTERM);
 struct timespec pause={0,10000000};
 for(int i=0;i<50;i++) {
  if(waitpid(*pid,NULL,WNOHANG)==*pid) { *pid=0;return; }
  nanosleep(&pause,NULL);
 }
 kill(-*pid,SIGKILL);while(waitpid(*pid,NULL,0)<0 && errno==EINTR) {} *pid=0;
}
static void decoder_stop(AudioBackend *b) {
 b->armed=0;
 if(b->input>=0) close(b->input);
 if(b->output>=0) close(b->output);
 b->input=b->output=-1;terminate(&b->decoder);b->used=0;b->working.playing=0;
}
static int spawn_group(pid_t *pid,const char *path,char *const argv[],posix_spawn_file_actions_t *files) {
 posix_spawnattr_t attr;posix_spawnattr_init(&attr);
 posix_spawnattr_setflags(&attr,POSIX_SPAWN_SETPGROUP);posix_spawnattr_setpgroup(&attr,0);
 int result=posix_spawn(pid,path,files,&attr,argv,environ);posix_spawnattr_destroy(&attr);
 return result;
}
static int command(AudioBackend *b,const char *text) {
 if(b->input<0) return -1;
 size_t left=strlen(text);
 while(left) { ssize_t n=write(b->input,text,left);if(n<0&&errno==EINTR) continue;if(n<=0)return -1;left-=(size_t)n;text+=n; }
 return 0;
}
static void route(AudioBackend *b,int bluetooth);
static void load_track(AudioBackend *b,int resume) {
 if(!b->route_ready) {route(b,b->working.bluetooth);return;}
 decoder_stop(b);const LibraryTrack *t=track(b);
 if(!t) { message(b,"NO MUSIC");b->want_play=0;return; }
 if(strchr(t->path,'\n')||strchr(t->path,'\r')) {message(b,"INVALID MUSIC PATH");b->want_play=0;return;}
 int to[2],from[2];if(pipe(to)) {message(b,"PLAYER PIPE ERROR");b->want_play=0;return;}
 if(pipe(from)) {close(to[0]);close(to[1]);message(b,"PLAYER PIPE ERROR");b->want_play=0;return;}
 for(int i=0;i<2;i++){fcntl(to[i],F_SETFD,FD_CLOEXEC);fcntl(from[i],F_SETFD,FD_CLOEXEC);}
 posix_spawn_file_actions_t files;posix_spawn_file_actions_init(&files);
 posix_spawn_file_actions_adddup2(&files,to[0],STDIN_FILENO);
 posix_spawn_file_actions_adddup2(&files,from[1],STDOUT_FILENO);
 for(int i=0;i<2;i++){posix_spawn_file_actions_addclose(&files,to[i]);posix_spawn_file_actions_addclose(&files,from[i]);}
 const char *path=getenv("KERNIX_MPG123");if(!path||!*path) path="/usr/bin/mpg123";
 char *args[]={(char *)path,"-R","-o","alsa","-a","default",NULL};
 int result=spawn_group(&b->decoder,path,args,&files);posix_spawn_file_actions_destroy(&files);
 close(to[0]);close(from[1]);
 if(result) {close(to[1]);close(from[0]);b->decoder=0;message(b,"CANNOT START MPG123");b->want_play=0;return;}
 b->input=to[1];b->output=from[0];fcntl(b->output,F_SETFL,O_NONBLOCK);
 b->decode_error=0;b->frame=resume;b->working.elapsed=resume?b->working.elapsed:0;
 if(!resume)b->working.duration=0;
 b->working.paused=!b->want_play;
 char request[4352];
 char volume[48];snprintf(volume,sizeof volume,"VOLUME %d\n",b->working.volume);
 command(b,volume);
 snprintf(request,sizeof request,"LOADPAUSED %s\nJUMP %d\n%s",t->path,resume,b->want_play?"PAUSE\n":"");
 if(command(b,request)) {message(b,"PLAYER COMMAND FAILED");decoder_stop(b);b->want_play=0;return;}
 message(b,b->want_play?"STARTING TRACK":"PAUSED");
}
static unsigned random_number(AudioBackend *b) { unsigned x=b->random;x^=x<<13;x^=x>>17;x^=x<<5;return b->random=x; }
static int advance(AudioBackend *b,int direction) {
 const LibraryPlaylist *p=playlist(b);if(!p||!p->count)return 0;
 int count=(int)p->count;
 if(!b->working.shuffle || direction<0) { b->bag_count=0;return (b->working.song+direction+count)%count; }
 if(count==1)return 0;
 if((size_t)count>b->bag_capacity){
  int *bag=realloc(b->bag,(size_t)count*sizeof *bag);
  if(!bag)return (b->working.song+1)%count;
  b->bag=bag;b->bag_capacity=(size_t)count;
 }
 if(!b->bag_count) {
  for(int i=0;i<count;i++) if(i!=b->working.song)b->bag[b->bag_count++]=i;
  for(int i=b->bag_count-1;i>0;i--){int j=(int)(random_number(b)%(unsigned)(i+1));int tmp=b->bag[i];b->bag[i]=b->bag[j];b->bag[j]=tmp;}
 }
 return b->bag[--b->bag_count];
}
static void eof(AudioBackend *b) {
 b->working.playing=0;
 if(!b->want_play||b->decode_error)return;
 if(!b->working.repeat)b->working.song=advance(b,1);
 load_track(b,0);
}
static void line(AudioBackend *b,const char *text) {
 int state;int frame,remaining;double elapsed,left;
 if(sscanf(text,"@F %d %d %lf %lf",&frame,&remaining,&elapsed,&left)==4) {
  b->frame=frame;b->working.elapsed=elapsed;b->working.duration=elapsed+left;
 } else if(sscanf(text,"@P %d",&state)==1) {
  if(state==2) { b->armed=1;b->working.playing=1;b->working.paused=0;message(b,"PLAYING"); }
  else if(state==1) { b->working.playing=0;b->working.paused=1;message(b,"PAUSED"); }
  else if(state==0 && b->armed) { b->armed=0;eof(b); }
 } else if(!strncmp(text,"@E",2)) {
  b->decode_error=1;b->want_play=0;b->working.playing=0;b->working.paused=1;message(b,text+3);decoder_stop(b);
 }
}
static void decoder_poll(AudioBackend *b) {
 if(b->output<0)return;
 int old=b->output;pid_t old_pid=b->decoder;
 for(int iteration=0;iteration<16;iteration++) {
  char data[1024];ssize_t n=read(old,data,sizeof data);
  if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))break;
  if(n<0&&errno==EINTR)continue;
  if(n<=0) {decoder_stop(b);b->want_play=0;b->working.paused=1;message(b,"PLAYER EXITED");break;}
  for(ssize_t i=0;i<n;i++) {
   if(data[i]=='\n') {b->incoming[b->used]=0;line(b,b->incoming);b->used=0;if(b->decoder!=old_pid)return;}
   else if(b->used+1<sizeof b->incoming)b->incoming[b->used++]=data[i];
  }
 }
}
static void route(AudioBackend *b,int bluetooth) {
 if(b->working.pairing){message(b,"WAIT FOR PAIRING");return;}
 terminate(&b->router);decoder_stop(b);b->resume_frame=b->frame;
 b->route_ready=0;b->working.switching=1;b->working.paused=1;
 message(b,bluetooth?"CONNECTING BLUETOOTH":"SWITCHING TO AUX");
 char *args[]={(char *)helper(),bluetooth?"bluetooth":"dac",NULL};
 int result=spawn_group(&b->router,helper(),args,NULL);
 if(result) {b->router=0;b->working.switching=0;message(b,"OUTPUT HELPER FAILED");return;}
 /* Store desired output until the helper confirms it. */
 b->resume_frame=b->frame;b->route_bluetooth=bluetooth;b->route_deadline=(long)time(NULL)+70;
}
static void pairing_command(AudioBackend *b,int pair) {
 if(b->working.pairing || (!pair && b->control>0))return;
 if(pair && b->working.switching){message(b,"WAIT FOR OUTPUT SWITCH");return;}
 terminate(&b->control);
 b->control_pair=pair;
 if(pair)b->working.pair_checked=0;
 if(pair){b->working.pairing=1;b->working.paired=0;message(b,"PAIRING - HEADPHONES IN PAIR MODE");}
 char *args[]={(char *)helper(),pair?"pair":"pair-status",NULL};
 int result=spawn_group(&b->control,helper(),args,NULL);
 if(result){b->control=0;b->working.pairing=0;b->working.paired=0;message(b,"CANNOT CHECK BLUETOOTH");return;}
 b->control_deadline=(long)time(NULL)+(pair?85:10);
}
static void pairing_poll(AudioBackend *b) {
 if(b->control>0){
  int status=0;pid_t done=waitpid(b->control,&status,WNOHANG);
  if(done==0 && (long)time(NULL)>b->control_deadline){
   terminate(&b->control);b->working.paired=0;b->working.pair_checked=1;b->working.pairing=0;
   if(b->control_pair)message(b,"PAIRING TIMED OUT - TRY AGAIN");
   b->next_pair_check=(long)time(NULL)+10;
  } else if(done==b->control){
   b->control=0;b->working.paired=WIFEXITED(status)&&WEXITSTATUS(status)==0;
   b->working.pair_checked=1;b->working.pairing=0;b->next_pair_check=(long)time(NULL)+10;
   if(b->control_pair)message(b,b->working.paired?"BLUETOOTH PAIRED":"PAIR FAILED - CHECK PAIR MODE");
  }
 }
 if(b->working.pair_checked && !b->control && !b->working.switching && (long)time(NULL)>=b->next_pair_check)
  pairing_command(b,0);
}
static void handle(AudioBackend *b,AudioCommand c) {
 switch(c.action) {
 case AUDIO_SELECT:
  if(c.playlist<0||(size_t)c.playlist>=b->library->playlist_count||c.song<0||(size_t)c.song>=b->library->playlists[c.playlist].count)break;
  b->working.playlist=c.playlist;b->working.song=c.song;b->bag_count=0;b->frame=b->resume_frame=0;b->want_play=1;
  if(!b->working.switching)load_track(b,0);
  break;
 case AUDIO_PAUSE:
  b->want_play=!b->want_play;
  if(!b->working.switching) {
   if(b->decoder) {if(command(b,"PAUSE\n")){message(b,"PAUSE COMMAND FAILED");b->want_play=0;}}
   else if(b->want_play)load_track(b,0);
  }
  break;
 case AUDIO_NEXT:case AUDIO_PREVIOUS:
  b->working.song=advance(b,c.action==AUDIO_NEXT?1:-1);b->frame=b->resume_frame=0;b->want_play=1;
  if(!b->working.switching)load_track(b,0);
  break;
 case AUDIO_REPEAT:b->working.repeat=c.value;break;
 case AUDIO_SHUFFLE:b->working.shuffle=c.value;b->working.mode_set=1;b->bag_count=0;
  message(b,c.value?"SHUFFLE MODE":"LINEAR MODE");break;
 case AUDIO_BLUETOOTH:route(b,1);break;
 case AUDIO_AUX:route(b,0);break;
 case AUDIO_VOLUME_UP:case AUDIO_VOLUME_DOWN: {
  int volume=b->working.volume+(c.action==AUDIO_VOLUME_UP?5:-5);
  if(volume<0)volume=0;
  if(volume>100)volume=100;
  char request[48];snprintf(request,sizeof request,"VOLUME %d\n",volume);
  if(b->decoder && command(b,request)){message(b,"VOLUME UPDATE FAILED");break;}
  b->working.volume=volume;
  break;
 }
 case AUDIO_PAIR:pairing_command(b,1);break;
 case AUDIO_PAIR_STATUS:pairing_command(b,0);break;
 }
}
static void *worker(void *context) {
 AudioBackend *b=context;
 for(;;) {
  AudioCommand c;int have=0,stop;
  pthread_mutex_lock(&b->lock);stop=b->stop;
  if(b->count){c=b->commands[b->head];b->head=(b->head+1)%32;b->count--;have=1;}
  pthread_mutex_unlock(&b->lock);
  if(stop)break;
  if(have)handle(b,c);
  if(b->router>0) {
   int status=0;pid_t done=waitpid(b->router,&status,WNOHANG);
   if(done==0 && (long)time(NULL)>b->route_deadline) {
    terminate(&b->router);b->working.switching=0;b->want_play=0;b->working.paused=1;
    message(b,"OUTPUT CONNECTION TIMED OUT");
   } else if(done==b->router) {
    b->router=0;b->working.switching=0;
    if(WIFEXITED(status)&&WEXITSTATUS(status)==0) {
     b->working.bluetooth=b->route_bluetooth;b->route_ready=1;
     if(track(b))load_track(b,b->resume_frame);else message(b,"OUTPUT READY - NO MUSIC");
    } else {
     b->want_play=0;b->working.paused=1;message(b,"OUTPUT FAILED - CHECK LOG");
    }
   }
  }
  pairing_poll(b);decoder_poll(b);publish(b);
  struct timespec delay={0,30000000};nanosleep(&delay,NULL);
 }
 terminate(&b->control);terminate(&b->router);decoder_stop(b);return NULL;
}
int audio_start(AudioBackend *b,const MusicLibrary *library) {
 memset(b,0,sizeof *b);b->library=library;b->input=b->output=-1;
 b->working.volume=25;b->working.paused=1;b->random=(unsigned)time(NULL)^(unsigned)getpid();if(!b->random)b->random=1;
 FILE *config=fopen("/var/lib/kernix-audio/asound.conf","r");
 if(config){char linebuf[128];if(fgets(linebuf,sizeof linebuf,config))b->working.bluetooth=strstr(linebuf,"output: bluetooth")!=NULL;fclose(config);}
 message(b,"READY");b->published=b->working;
 if(pthread_mutex_init(&b->lock,NULL))return -1;
 /* Pipe failure must return EPIPE instead of killing the UI process. */
 signal(SIGPIPE,SIG_IGN);
 int result=pthread_create(&b->thread,NULL,worker,b);
 if(result){pthread_mutex_destroy(&b->lock);return -1;}
 b->initialized=1;return 0;
}
int audio_send(AudioBackend *b,AudioCommand c) {
 if(!b->initialized)return -1;
 pthread_mutex_lock(&b->lock);
 if(b->count==32){pthread_mutex_unlock(&b->lock);return -1;}
 b->commands[(b->head+b->count)%32]=c;b->count++;pthread_mutex_unlock(&b->lock);return 0;
}
void audio_snapshot(AudioBackend *b,AudioState *s) {
 if(!b->initialized){memset(s,0,sizeof *s);s->paused=1;snprintf(s->message,sizeof s->message,"AUDIO UNAVAILABLE");return;}
 pthread_mutex_lock(&b->lock);*s=b->published;pthread_mutex_unlock(&b->lock);
}
void audio_shutdown(AudioBackend *b) {
 if(!b->initialized)return;
 pthread_mutex_lock(&b->lock);b->stop=1;pthread_mutex_unlock(&b->lock);
 pthread_join(b->thread,NULL);free(b->bag);b->bag=NULL;b->bag_capacity=0;pthread_mutex_destroy(&b->lock);b->initialized=0;
}
