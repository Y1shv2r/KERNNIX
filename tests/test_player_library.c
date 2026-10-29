#define main kernix_application_main
#include "../ui/calibrate.c"
#undef main
#include <assert.h>
int main(int argc,char **argv) {
 char lines[16][24];
 assert(artist_wrap("",lines)==0);
 assert(artist_wrap("Chaar Diwaari ft. Indian Ocean, Gini",lines)==2);
 assert(!strcmp(lines[0],"Chaar Diwaari ft."));
 assert(!strcmp(lines[1],"Indian Ocean, Gini"));
 char long_artist[256];memset(long_artist,'A',255);long_artist[255]=0;
 int line_count=artist_wrap(long_artist,lines);assert(line_count==12);
 size_t total=0;for(int i=0;i<line_count;i++){assert(strlen(lines[i])<=23);total+=strlen(lines[i]);}
 assert(total==255);
 /* Idle at exactly 30s; wake consumes the gesture and preserves the page. */
 player_state.view=VIEW_CONTROLS;player_state.page=2;
 player_activity(1000);player_idle_tick(30999);assert(!player_idle);
 player_idle_tick(31000);assert(player_idle);
 assert(player_activity(31001)==1);assert(!player_idle);
 assert(player_state.view==VIEW_CONTROLS && player_state.page==2);
 assert(player_activity(32000)==0);
 player_idle_tick(61999);assert(!player_idle);
 player_idle_tick(62000);assert(player_idle);
 player_activity(62001);player_state.view=VIEW_PLAYER;player_state.page=0;
 assert(argc==2);char error[512];
 assert(library_load(&music_library,argv[1],error,sizeof error)==0);
 v.xres=320;v.yres=240;v.bits_per_pixel=32;
 v.red.offset=16;v.red.length=8;v.green.offset=8;v.green.length=8;v.blue.length=8;
 f.line_length=1280;f.smem_len=1280*240;pixels=calloc(1,f.smem_len);assert(pixels);
 /* Output label reflects the active route; tapping selects the other one. */
 assert(pthread_mutex_init(&audio_backend.lock,NULL)==0);
 audio_backend.initialized=1;
 audio_backend.published.bluetooth=0;player_audio_sync();assert(player_state.aux);
 player_click(170,90);assert(audio_backend.commands[0].action==AUDIO_BLUETOOTH);
 audio_backend.published.bluetooth=1;player_audio_sync();assert(!player_state.aux);
 player_click(170,90);assert(audio_backend.commands[1].action==AUDIO_AUX);
 /* Animation opens controls; all three touch targets queue backend work. */
 player_click(70,70);assert(player_state.view==VIEW_CONTROLS);
 assert(audio_backend.commands[2].action==AUDIO_PAIR_STATUS);
 player_click(100,60);assert(audio_backend.commands[3].action==AUDIO_VOLUME_UP);
 player_click(100,120);assert(audio_backend.commands[4].action==AUDIO_VOLUME_DOWN);
 audio_state.pairing=0;player_click(100,175);assert(audio_backend.commands[5].action==AUDIO_PAIR);
 /* A complete wake tap must not activate the control under the finger. */
 int touch_pipe[2];assert(pipe(touch_pipe)==0);
 assert(fcntl(touch_pipe[0],F_SETFL,O_NONBLOCK)==0);
 struct player_touch input={.fd=touch_pipe[0]};
 input.ax.maximum=4095;input.ay.maximum=4095;
 struct input_event events[4]={0};
 events[0].type=EV_KEY;events[0].code=BTN_TOUCH;events[0].value=1;
 events[1].type=EV_SYN;events[1].code=SYN_REPORT;
 events[2].type=EV_KEY;events[2].code=BTN_TOUCH;
 events[3].type=EV_SYN;events[3].code=SYN_REPORT;
 unsigned before_wake=audio_backend.count;player_idle=1;
 assert(write(touch_pipe[1],events,sizeof events)==sizeof events);
 player_touch_poll(&input);
 assert(!player_idle && audio_backend.count==before_wake);
 assert(player_state.view==VIEW_CONTROLS && !input.active);
 close(touch_pipe[0]);close(touch_pipe[1]);
 unsigned count=audio_backend.count;
 audio_state.pairing=1;player_click(100,175);assert(audio_backend.count==count);
 audio_state.paired=1;audio_state.pair_checked=1;assert(pairing_button_color()==0);
 audio_state.pairing=0;assert(pairing_button_color()==0x167a45);
 audio_state.paired=0;assert(pairing_button_color()==0);
 audio_state.volume=25;player_render(0);
 player_click(40,20);assert(player_state.view==VIEW_PLAYER);
 audio_backend.initialized=0;pthread_mutex_destroy(&audio_backend.lock);
 player_idle=1;player_render(0);player_idle=0;
 player_render(0);
 for(size_t p=0;p<music_library.playlist_count;p++) {
  player_state.view=VIEW_PLAYER;player_click(170,20);
  for(size_t page=0;page<p/5;page++) player_click(306,228);
  player_click(10,(int)(p%5)*48+20);
  assert(player_state.view==VIEW_SONGS && player_state.browsing_playlist==(int)p);
  player_render(0);
  size_t count=music_library.playlists[p].count;
  for(size_t song=0;song<count;song++) {
   player_state.view=VIEW_SONGS;player_state.browsing_playlist=(int)p;player_state.page=(int)(song/5);
   player_click(10,(int)(song%5)*48+20);
   assert(player_state.view==VIEW_PLAYER && selected_track()==&music_library.tracks[music_library.playlists[p].first+song]);
   player_render(1);
  }
  player_state.view=VIEW_SONGS;player_state.page=(int)((count-1)/5);
  player_click(306,228);assert(player_state.page==(int)((count-1)/5));
  if(count%5) {
   player_click(10,(int)(count%5)*48+20);assert(player_state.view==VIEW_SONGS);
  }
 }
 free(pixels);library_free(&music_library);puts("Database-backed selection and dynamic page limits passed");return 0;
}
