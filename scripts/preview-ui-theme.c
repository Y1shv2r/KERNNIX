#define main kernix_application_main
#include "../ui/calibrate.c"
#undef main
static void save(const char *path){
 FILE *out=fopen(path,"wb");fprintf(out,"P6\n320 240\n255\n");
 for(int y=0;y<240;y++)for(int x=0;x<320;x++){
  unsigned n=((unsigned *)(pixels+y*1280))[x];unsigned char rgb[3]={n>>16,n>>8,n};fwrite(rgb,1,3,out);
 }
 fclose(out);
}
int main(void){
 v.xres=320;v.yres=240;v.bits_per_pixel=32;v.red.offset=16;v.red.length=8;v.green.offset=8;v.green.length=8;v.blue.length=8;
 f.line_length=1280;f.smem_len=1280*240;pixels=calloc(1,f.smem_len);
 LibraryPlaylist playlist={.name="01",.first=0,.count=3};LibraryTrack tracks[3]={0};
 snprintf(tracks[0].title,sizeof tracks[0].title,"Daddy Cop");snprintf(tracks[0].artist,sizeof tracks[0].artist,"Zayder Harley");snprintf(tracks[0].id,sizeof tracks[0].id,"0101");
 snprintf(tracks[1].title,sizeof tracks[1].title,"Tum Hi Ho");snprintf(tracks[2].title,sizeof tracks[2].title,"Ilahi");
 music_library.playlists=&playlist;music_library.playlist_count=1;music_library.tracks=tracks;music_library.track_count=3;
 player_state.aux=0;audio_state.elapsed=74;audio_state.duration=292;audio_state.volume=25;
 player_render(0);save("/tmp/kernix-player-theme.ppm");
 player_idle=1;player_render(0);save("/tmp/kernix-idle-theme.ppm");player_idle=0;
 player_state.view=VIEW_CONTROLS;player_render(0);save("/tmp/kernix-controls-theme.ppm");
 audio_state.pair_checked=1;audio_state.paired=1;player_render(0);save("/tmp/kernix-paired-theme.ppm");
 player_state.view=VIEW_SONGS;player_render(0);save("/tmp/kernix-list-theme.ppm");
 free(pixels);return 0;
}
