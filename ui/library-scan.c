#include "library.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
 if(argc==1 || (argc==3 && !strcmp(argv[1],"--validate"))){
  MusicLibrary l={0};char error[512];
  if(library_load(&l,argc==1?KERNIX_DATABASE:argv[2],error,sizeof error)){fprintf(stderr,"%s\n",error);return 1;}
  printf("Library: %zu playlists, %zu memberships\n",l.playlist_count,l.track_count);library_free(&l);return 0;
 }
 if(argc!=1 && argc!=3) { fprintf(stderr,"Usage: %s [music-folder database]\n",argv[0]);return 2; }
 char error[512];const char *music=argc==3?argv[1]:KERNIX_MUSIC,*db=argc==3?argv[2]:KERNIX_DATABASE;
 if(library_scan(music,db,error,sizeof error)) { fprintf(stderr,"%s\n",error);return 1; }
 MusicLibrary l={0};if(library_load(&l,db,error,sizeof error)) { fprintf(stderr,"%s\n",error);return 1; }
 printf("Library: %zu playlists, %zu tracks\n",l.playlist_count,l.track_count);library_free(&l);return 0;
}
