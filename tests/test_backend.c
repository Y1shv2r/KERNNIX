#define _POSIX_C_SOURCE 200809L
#include "../ui/backend.c"
#include <assert.h>
static void delay(void) { struct timespec t={0,20000000};nanosleep(&t,NULL); }
static AudioState state(AudioBackend *b) { AudioState s;audio_snapshot(b,&s);return s; }
static AudioState wait_state(AudioBackend *b,int song,int playing,int switching) {
 for(int i=0;i<200;i++) { AudioState s=state(b);if(s.song==song&&s.playing==playing&&s.switching==switching)return s;delay(); }
 AudioState s=state(b);fprintf(stderr,"Unexpected state song=%d playing=%d switching=%d message=%s\n",s.song,s.playing,s.switching,s.message);abort();
}
static void send_action(AudioBackend *b,AudioAction action,int song,int value) { AudioCommand c={action,0,song,value};assert(audio_send(b,c)==0); }
static void trigger(const char *variable) { FILE *f=fopen(getenv(variable),"w");assert(f);fclose(f); }
static int loads(void) { int n=0;char linebuf[512];FILE *f=fopen(getenv("TEST_AUDIO_LOG"),"r");if(!f)return 0;while(fgets(linebuf,sizeof linebuf,f))if(!strncmp(linebuf,"LOADPAUSED",10))n++;fclose(f);return n; }
int main(void) {
 LibraryPlaylist p={.id="01",.name="Test",.first=0,.count=3};
 LibraryTrack tracks[3]={0};for(int i=0;i<3;i++)snprintf(tracks[i].path,sizeof tracks[i].path,"/music/01%02d.mp3",i+1);
 MusicLibrary l={&p,tracks,1,3};AudioBackend b;
 assert(audio_start(&b,&l)==0);assert(state(&b).paused);
 send_action(&b,AUDIO_SELECT,0,0);wait_state(&b,0,1,0);assert(b.route_ready);
 send_action(&b,AUDIO_PAUSE,0,0);assert(wait_state(&b,0,0,0).paused);
 send_action(&b,AUDIO_PAUSE,0,0);wait_state(&b,0,1,0);
 send_action(&b,AUDIO_REPEAT,0,1);for(int i=0;i<100&&!state(&b).repeat;i++)delay();assert(state(&b).repeat);
 int before=loads();trigger("TEST_EOF_FILE");for(int i=0;i<100&&loads()==before;i++)delay();assert(loads()>before);wait_state(&b,0,1,0);
 send_action(&b,AUDIO_REPEAT,0,0);for(int i=0;i<100&&state(&b).repeat;i++)delay();assert(!state(&b).repeat);
 trigger("TEST_EOF_FILE");wait_state(&b,1,1,0);
 send_action(&b,AUDIO_NEXT,0,0);wait_state(&b,2,1,0);
 send_action(&b,AUDIO_PREVIOUS,0,0);wait_state(&b,1,1,0);
 send_action(&b,AUDIO_BLUETOOTH,0,0);wait_state(&b,1,0,1);wait_state(&b,1,1,0);assert(state(&b).bluetooth);
 send_action(&b,AUDIO_AUX,0,0);wait_state(&b,1,0,1);wait_state(&b,1,1,0);assert(!state(&b).bluetooth);
 /* A paused song must stay paused when either output is selected. */
 send_action(&b,AUDIO_PAUSE,0,0);wait_state(&b,1,0,0);
 send_action(&b,AUDIO_BLUETOOTH,0,0);wait_state(&b,1,0,1);wait_state(&b,1,0,0);
 assert(state(&b).bluetooth && state(&b).paused);
 send_action(&b,AUDIO_AUX,0,0);wait_state(&b,1,0,1);wait_state(&b,1,0,0);
 assert(!state(&b).bluetooth && state(&b).paused);
 send_action(&b,AUDIO_PAUSE,0,0);wait_state(&b,1,1,0);
 trigger("TEST_FAIL_FILE");send_action(&b,AUDIO_BLUETOOTH,0,0);wait_state(&b,1,0,1);
 wait_state(&b,1,0,0);assert(!state(&b).bluetooth);assert(strstr(state(&b).message,"FAILED"));
 unlink(getenv("TEST_FAIL_FILE"));send_action(&b,AUDIO_SELECT,0,0);wait_state(&b,0,1,0);
 /* Cancelling connection for AUX must preserve the selected song. */
 send_action(&b,AUDIO_BLUETOOTH,0,0);wait_state(&b,0,0,1);
 send_action(&b,AUDIO_SELECT,2,0);send_action(&b,AUDIO_AUX,0,0);
 wait_state(&b,2,1,0);assert(!state(&b).bluetooth);
 /* Volume stays bounded, applies to mpg123 and survives output changes. */
 for(int i=0;i<25;i++){send_action(&b,AUDIO_VOLUME_UP,0,0);delay();}
 for(int i=0;i<100&&state(&b).volume!=100;i++)delay();
 assert(state(&b).volume==100);
 for(int i=0;i<25;i++){send_action(&b,AUDIO_VOLUME_DOWN,0,0);delay();}
 for(int i=0;i<100&&state(&b).volume!=0;i++)delay();
 assert(state(&b).volume==0);
 send_action(&b,AUDIO_VOLUME_UP,0,0);for(int i=0;i<100&&state(&b).volume!=5;i++)delay();assert(state(&b).volume==5);
 /* Pairing runs beside playback and reflects confirmed helper results. */
 send_action(&b,AUDIO_PAIR,0,0);
 for(int i=0;i<100&&!state(&b).pairing;i++)delay();
 assert(state(&b).pairing);
 assert(state(&b).playing);
 for(int i=0;i<100&&state(&b).pairing;i++)delay();
 assert(state(&b).paired&&state(&b).pair_checked);
 trigger("TEST_PAIR_FAIL_FILE");send_action(&b,AUDIO_PAIR,0,0);
 for(int i=0;i<100&&!state(&b).pairing;i++)delay();
 for(int i=0;i<100&&state(&b).pairing;i++)delay();
 assert(!state(&b).paired&&!state(&b).pairing);
 unlink(getenv("TEST_PAIR_FAIL_FILE"));
 audio_shutdown(&b);assert(!b.initialized && b.decoder==0 && b.router==0);
 AudioBackend order={0};order.library=&l;order.working.shuffle=1;order.random=12345;
 int seen=0;for(int i=0;i<2;i++){order.working.song=advance(&order,1);assert(order.working.song!=0);seen|=1<<order.working.song;}assert(seen==6);
 p.count=150;order.bag_count=0;
 for(int i=0;i<149;i++)order.working.song=advance(&order,1);
 assert(order.bag_capacity>=150);
 p.count=1;order.bag_count=0;assert(advance(&order,1)==0);
 free(order.bag);
 puts("Pause/resume, EOF linear/repeat, shuffled cycle, output success/failure/cancel and shutdown passed");
}
