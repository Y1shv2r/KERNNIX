#include "library.h"
#include "backend.h"
static AudioBackend audio_backend;
static AudioState audio_state;
static void player_action(AudioAction action,int playlist,int song,int value) {
 AudioCommand command={action,playlist,song,value};
 if(audio_send(&audio_backend,command)) fprintf(stderr,"Audio command queue unavailable/full\n");
}
static MusicLibrary music_library;
enum player_view { VIEW_PLAYER, VIEW_PLAYLISTS, VIEW_SONGS, VIEW_CONTROLS };
static struct {
 enum player_view view;
 int page, browsing_playlist, playlist, song, paused, aux, shuffle, repeat, mode_target;
} player_state;
static const LibraryTrack *selected_track(void) {
 if(player_state.playlist<0 || (size_t)player_state.playlist>=music_library.playlist_count) return NULL;
 const LibraryPlaylist *p=&music_library.playlists[player_state.playlist];
 if(player_state.song<0 || (size_t)player_state.song>=p->count) return NULL;
 return &music_library.tracks[p->first+(size_t)player_state.song];
}
static size_t list_count(void) {
 if(player_state.view==VIEW_PLAYLISTS) return music_library.playlist_count;
 if(player_state.browsing_playlist<0 || (size_t)player_state.browsing_playlist>=music_library.playlist_count) return 0;
 return music_library.playlists[player_state.browsing_playlist].count;
}
/* Monotonic inactivity overlay; underlying view and playback stay intact. */
static uint64_t player_last_touch;
static int player_idle;
static uint64_t player_now_ms(void) {
 struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
 return (uint64_t)now.tv_sec*1000+(uint64_t)now.tv_nsec/1000000;
}
static int player_activity(uint64_t now) {
 int consumed=player_idle;player_idle=0;player_last_touch=now;return consumed;
}
static void player_idle_tick(uint64_t now) {
 if(now>=player_last_touch && now-player_last_touch>=30000)player_idle=1;
}
static void player_click(int x,int y) {
 if(x<0 || x>=320 || y<0 || y>=240) return;
 if(player_state.view==VIEW_CONTROLS) {
  if(y<40 && x<110){player_state.view=VIEW_PLAYER;return;}
  if(x>=20 && x<300){
   if(y>=48 && y<94)player_action(AUDIO_VOLUME_UP,0,0,0);
   else if(y>=104 && y<150)player_action(AUDIO_VOLUME_DOWN,0,0,0);
   else if(y>=160 && y<206 && !audio_state.pairing)player_action(AUDIO_PAIR,0,0,0);
  }
  return;
 }
 if(player_state.view==VIEW_PLAYER) {
  if(x>=8 && x<154 && y>=8 && y<154){
   player_state.view=VIEW_CONTROLS;player_action(AUDIO_PAIR_STATUS,0,0,0);
  } else if(x>=162 && x<312 && y>=8 && y<72) {
   player_state.view=VIEW_PLAYLISTS; player_state.page=0;
  } else if(x>=162 && x<312 && y>=80 && y<114) {
   player_action(player_state.aux?AUDIO_BLUETOOTH:AUDIO_AUX,0,0,0);
  } else if(x>=162 && x<312 && y>=120 && y<154) {
   player_action(AUDIO_SHUFFLE,0,0,player_state.mode_target);
  } else if(x>=8 && x<312 && y>=190 && y<232) {
   if(x<84) player_action(AUDIO_REPEAT,0,0,!player_state.repeat);
   else if(x<160) player_action(AUDIO_PREVIOUS,0,0,0);
   else if(x<236) player_action(AUDIO_PAUSE,0,0,0);
   else player_action(AUDIO_NEXT,0,0,0);
  }
  return;
 }
 if(x>=272) {
  if(y<48 && player_state.page>0) player_state.page--;
  else if(y>=192 && (size_t)(player_state.page+1)*5<list_count()) player_state.page++;
  return;
 }
 int selected=player_state.page*5+y/48;
 if((size_t)selected>=list_count()) return;
 if(player_state.view==VIEW_PLAYLISTS) {
  player_state.browsing_playlist=selected;
  player_state.view=VIEW_SONGS; player_state.page=0;
 } else {
  player_state.playlist=player_state.browsing_playlist;
  player_state.song=selected;
  player_action(AUDIO_SELECT,player_state.playlist,selected,0);
  player_state.view=VIEW_PLAYER; player_state.page=0;
 }
}
static void player_audio_sync(void) {
 audio_snapshot(&audio_backend,&audio_state);
 player_state.playlist=audio_state.playlist;player_state.song=audio_state.song;
 player_state.paused=audio_state.paused;player_state.repeat=audio_state.repeat;
 player_state.shuffle=audio_state.shuffle;player_state.aux=!audio_state.bluetooth;
 if(audio_state.mode_set)player_state.mode_target=!audio_state.shuffle;
}
/* Framebuffer UI consumes confirmed playback snapshots. */
static void boxfill(int x,int y,int w,int h,unsigned c) {
 for(int j=y;j<y+h;j++) for(int i=x;i<x+w;i++) pixel(i,j,c);
}
static void wedge(int x,int y,int w,int h,int right,unsigned c) {
 for(int i=0;i<w;i++) {
  int half=(h/2)*(w-1-i)/(w>1?w-1:1);
  boxfill(right?x+i:x+w-1-i,y+h/2-half,1,half*2+1,c);
 }
}
#include "theme.h"
static void list_render(void) {
 const unsigned white=THEME_TEXT;
 theme_background();
 for(int row=0;row<5;row++) {
  size_t index=(size_t)player_state.page*5+(size_t)row;
  if(index>=list_count()) break;
  const char *name=player_state.view==VIEW_PLAYLISTS ? music_library.playlists[index].name :
   music_library.tracks[music_library.playlists[player_state.browsing_playlist].first+index].title;
  char title[22]; snprintf(title,sizeof title,"%.21s",name);
  int selected=player_state.view==VIEW_PLAYLISTS ? (int)index==player_state.playlist :
   player_state.browsing_playlist==player_state.playlist && (int)index==player_state.song;
  theme_card(3,row*48+3,265,43,selected?0x202629:0x0d171d,0x060c10,selected?THEME_SELECTED_EDGE:0x26343e);
  if(selected)boxfill(5,row*48+12,2,24,THEME_CREAM);
  theme_text(10,row*48+17,title,2,white);
 }
 if(!list_count()) theme_text(10,110,"NO MUSIC",2,white);
 /* Small 13x7 arrows; retain generous touch targets. */
 for(int i=0;i<7;i++) {
  if(player_state.page>0) boxfill(306-i,8+i,2*i+1,1,white);
  if((size_t)(player_state.page+1)*5<list_count()) boxfill(300+i,225+i,13-2*i,1,white);
 }
}
static unsigned pairing_button_color(void) {
 return audio_state.pair_checked && audio_state.paired && !audio_state.pairing ? 0x167a45 : 0x000000;
}
static void controls_render(void) {
 const unsigned white=THEME_TEXT;
 theme_background();
 boxfill(12,36,296,1,0x26343e);
 theme_text(12,14,"BACK",2,white);
 char volume[32];snprintf(volume,sizeof volume,"VOL %d",audio_state.volume);
 theme_text(175,17,volume,1,white);
 const char *labels[]={"VOLUME UP","VOLUME DOWN",audio_state.pairing?"PAIRING...":"PAIR BLUETOOTH"};
 for(int i=0;i<3;i++){
  int y=48+i*56;
  unsigned fill=i==2?pairing_button_color():0x0b141a;
  theme_card(20,y,280,46,fill,i==2?fill:0x050b0f,i==2&&fill?THEME_PAIRED_EDGE:THEME_EDGE);
  theme_text(160-(int)strlen(labels[i])*6,y+16,labels[i],2,white);
 }
 char status[51];snprintf(status,sizeof status,"%.50s",audio_state.message);
 theme_text(10,220,status,1,white);
}
/* Wrap artist metadata across the former artist/ID area; long names page
 * through all remaining lines instead of being permanently truncated. */
static int artist_wrap(const char *value,char lines[16][24]) {
 int count=0;
 while(*value && count<16){
  while(*value==' '||*value=='\n'||*value=='\r')value++;
  if(!*value)break;
  size_t n=0;while(value[n] && value[n]!='\n' && value[n]!='\r' && n<23)n++;
  size_t take=n;
  if(n==23 && value[n] && value[n]!=' '){
   for(size_t j=n;j>0;j--)if(value[j-1]==' '){take=j-1;break;}
   if(!take)take=n;
  }
  memcpy(lines[count],value,take);lines[count][take]=0;
  count++;value+=take;
 }
 return count;
}
#include "assets/crimson-idle.h"
static void idle_render(void) {
 for(int y=0;y<240;y++)for(int x=0;x<320;x++){
  unsigned raw=idle_art[y*320+x];
  pixel(x,y,(((raw>>11)&31)*255/31)<<16 | (((raw>>5)&63)*255/63)<<8 | (raw&31)*255/31);
 }
 theme_text(94,85,"K E R N I X",2,THEME_TEXT);
 const LibraryTrack *track=selected_track();
 const char *title=track ? track->title : "NO MUSIC";
 /* Wrap the title rather than hiding the end of a long song name. */
 char lines[16][24];int count=artist_wrap(title,lines);
 int pages=(count+1)/2;
 int page=pages ? (int)((player_now_ms()/5000)%(unsigned)pages) : 0;
 for(int row=0;row<2 && page*2+row<count;row++){
  const char *line=lines[page*2+row];
  theme_text(160-(int)strlen(line)*3,174+row*12,line,1,THEME_TEXT);
 }
 boxfill(45,216,230,4,0x303235);
 double fraction=audio_state.duration>0 ? audio_state.elapsed/audio_state.duration : 0;
 if(fraction<0)fraction=0;
 if(fraction>1)fraction=1;
 boxfill(45,216,(int)(230*fraction),4,THEME_CREAM);
}
static void player_render(unsigned frame) {
 if(player_idle){idle_render();return;}
 if(player_state.view==VIEW_CONTROLS){controls_render();return;}
 if(player_state.view!=VIEW_PLAYER) { list_render(); return; }
 const unsigned cream=THEME_CREAM,white=THEME_TEXT;
 theme_background();
 theme_card(8,8,146,146,0x14212e,0x050b12,0x465160);
 theme_artwork(frame);
 theme_card(162,8,150,64,0x111b21,0x070e12,THEME_EDGE);
 const LibraryTrack *track=selected_track();
 char details[64];
 snprintf(details,sizeof details,"%.23s",track ? track->title : "NO MUSIC"); theme_text(169,16,details,1,white);
 char artist_lines[16][24];
 int artist_count=artist_wrap(track ? track->artist : "ADD NUMBERED MP3 FILES",artist_lines);
 int pages=(artist_count+2)/3;
 int page=pages ? (int)((frame/50)%(unsigned)pages) : 0;
 for(int row=0;row<3 && page*3+row<artist_count;row++)
  theme_text(169,33+row*12,artist_lines[page*3+row],1,THEME_MUTED);
 theme_card(162,80,150,34,0x0e1a27,0x070e16,player_state.aux?THEME_EDGE:THEME_OUTPUT_EDGE);
 const char *output=audio_state.switching ? "SWITCHING" : player_state.aux ? "AUX" : "BLUETOOTH";
 int output_scale=audio_state.switching?1:2;
 theme_text(237-(int)strlen(output)*3*output_scale,90,output,output_scale,player_state.aux?cream:THEME_BLUE);
 theme_card(162,120,150,34,0x101b1b,0x070f10,THEME_MODE_EDGE);
 const char *mode=player_state.mode_target ? "SHUFFLE" : "LINEAR";
 theme_text(237-(int)strlen(mode)*6,130,mode,2,THEME_MODE_TEXT);
 /* Shared progress row spans beneath both panels. */
 int elapsed=(int)audio_state.elapsed,duration=(int)audio_state.duration;
 if(elapsed<0)elapsed=0;
 if(duration<0)duration=0;
 char clocktext[32];snprintf(clocktext,sizeof clocktext,"%d:%02d",elapsed/60,elapsed%60);theme_text(8,170,clocktext,1,cream);
 snprintf(clocktext,sizeof clocktext,"%d:%02d",duration/60,duration%60);theme_text(282,170,clocktext,1,cream);
 int progress=duration>0 ? (int)(226.0*elapsed/duration) : 0;if(progress>226)progress=226;
 boxfill(44,174,230,3,0x28353d);
 boxfill(44,174,progress,3,cream);
 boxfill(44+progress,171,4,9,cream);
 if(audio_state.message[0] && strcmp(audio_state.message,"PLAYING") && strcmp(audio_state.message,"PAUSED")) {
  char status[52];snprintf(status,sizeof status,"%.50s",audio_state.message);theme_text(8,157,status,1,white);
 }
 for(int i=0;i<4;i++) {
  unsigned top=i==2?THEME_PLAY_TOP:0x111b21,bottom=i==2?THEME_PLAY_BOTTOM:0x050b10;
  theme_card(8+i*76,190,74,42,top,bottom,i==2?THEME_PLAY_EDGE:THEME_EDGE);
 }
 /* Repeat, previous, pause, next: four equal 76px sections. */
 unsigned repeat_color=player_state.repeat ? cream : white;
 boxfill(33,200,22,3,repeat_color); boxfill(53,201,3,10,repeat_color);
 boxfill(36,218,22,3,repeat_color); boxfill(33,210,3,10,repeat_color);
 wedge(28,196,9,11,0,repeat_color); wedge(54,214,9,11,1,repeat_color);
 boxfill(103,200,4,23,white);
 wedge(109,199,13,25,0,white); wedge(123,199,13,25,0,white);
 if(player_state.paused) wedge(186,198,23,27,1,THEME_PLAY_ICON);
 else { boxfill(184,199,7,25,THEME_PLAY_ICON); boxfill(199,199,7,25,THEME_PLAY_ICON); }
 wedge(258,199,13,25,1,white); wedge(272,199,13,25,1,white);
 boxfill(287,200,4,23,white);
}
