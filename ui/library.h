#ifndef KERNIX_LIBRARY_H
#define KERNIX_LIBRARY_H
#include <stddef.h>
#define KERNIX_DATABASE "/mnt/music/current/library.db"
#define KERNIX_MUSIC "/usr/share/kernix/music"
typedef struct { char id[6],title[256],artist[256],album[256],path[4096]; int position; } LibraryTrack;
typedef struct { char id[3],name[256]; size_t first,count; } LibraryPlaylist;
typedef struct { LibraryPlaylist *playlists; LibraryTrack *tracks; size_t playlist_count,track_count; } MusicLibrary;
int library_scan(const char *music,const char *database,char *error,size_t capacity);
int library_load(MusicLibrary *library,const char *database,char *error,size_t capacity);
void library_free(MusicLibrary *library);
#endif
