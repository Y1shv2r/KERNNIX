#define _POSIX_C_SOURCE 200809L
#include "library.h"
#include <sqlite3.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
static int sql(sqlite3 *db,const char *query) { return sqlite3_exec(db,query,NULL,NULL,NULL)==SQLITE_OK ? 0 : -1; }
static void copy_column(char *out,size_t size,sqlite3_stmt *s,int column) {
 const unsigned char *value=sqlite3_column_text(s,column);
 snprintf(out,size,"%s",value ? (const char *)value : "");
}
static int open_database(sqlite3 **db,const char *path,int create,char *error,size_t size) {
 int flags=create ? SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE : SQLITE_OPEN_READONLY;
 if(sqlite3_open_v2(path,db,flags,NULL)!=SQLITE_OK) goto fail;
 sqlite3_busy_timeout(*db,1000);
 if(sql(*db,"PRAGMA foreign_keys=ON")) goto fail;
 sqlite3_stmt *s=NULL;
 if(sqlite3_prepare_v2(*db,"PRAGMA user_version",-1,&s,NULL)!=SQLITE_OK) goto fail;
 int version=sqlite3_step(s)==SQLITE_ROW ? sqlite3_column_int(s,0) : -1;
 sqlite3_finalize(s);
 if(version==0 && create) {
  if(sqlite3_prepare_v2(*db,"SELECT count(*) FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%'",-1,&s,NULL)!=SQLITE_OK) goto fail;
  int empty=sqlite3_step(s)==SQLITE_ROW && sqlite3_column_int(s,0)==0; sqlite3_finalize(s);
  if(!empty) { snprintf(error,size,"Refusing unversioned nonempty database"); return -1; }
  if(sql(*db,"BEGIN IMMEDIATE;"
   "CREATE TABLE playlists(id TEXT PRIMARY KEY NOT NULL CHECK(length(id)=2 AND id NOT GLOB '*[^0-9]*' AND id<>'00'),name TEXT NOT NULL);"
   "CREATE TABLE tracks(id TEXT PRIMARY KEY NOT NULL CHECK(length(id)=4 AND id NOT GLOB '*[^0-9]*'),"
   "playlist_id TEXT NOT NULL REFERENCES playlists(id),position INTEGER NOT NULL CHECK(position BETWEEN 1 AND 99),"
   "file_path TEXT NOT NULL UNIQUE,title TEXT NOT NULL,artist TEXT NOT NULL DEFAULT 'Unknown Artist',"
   "album TEXT NOT NULL DEFAULT 'Unknown Album',duration_ms INTEGER NOT NULL DEFAULT 0 CHECK(duration_ms>=0),"
   "available INTEGER NOT NULL DEFAULT 1 CHECK(available IN (0,1)),"
   "CHECK(substr(id,1,2)=playlist_id AND substr(id,3,2)=printf('%02d',position)),UNIQUE(playlist_id,position));"
   "CREATE INDEX tracks_available_order ON tracks(available,playlist_id,position);"
   "PRAGMA user_version=1; COMMIT;")) goto fail;
 } else if(version!=1 && !(version==2 && !create)) { snprintf(error,size,"Unsupported library schema version %d",version); return -1; }
 return 0;
fail:
 snprintf(error,size,"SQLite: %s",*db ? sqlite3_errmsg(*db) : "open failed");return -1;
}
int library_scan(const char *music,const char *database,char *error,size_t size) {
 DIR *dir=opendir(music); if(!dir) { snprintf(error,size,"Music folder: %s",strerror(errno));return -1; }
 sqlite3 *db=NULL;sqlite3_stmt *playlist=NULL,*track=NULL; int result=-1,transaction=0;
 if(open_database(&db,database,1,error,size)) goto done;
 if(sql(db,"BEGIN IMMEDIATE")) goto sql_error;
 transaction=1;
 if(sql(db,"UPDATE tracks SET available=0")) goto sql_error;
 if(sqlite3_prepare_v2(db,"INSERT INTO playlists(id,name) VALUES(?,?) ON CONFLICT(id) DO NOTHING",-1,&playlist,NULL)!=SQLITE_OK ||
    sqlite3_prepare_v2(db,"INSERT INTO tracks(id,playlist_id,position,file_path,title) VALUES(?,?,?,?,?) "
    "ON CONFLICT(id) DO UPDATE SET file_path=excluded.file_path,available=1 WHERE tracks.available=0",-1,&track,NULL)!=SQLITE_OK) goto sql_error;
 for(;;) {
  errno=0;struct dirent *entry=readdir(dir);
  if(!entry) { if(errno) { snprintf(error,size,"Music scan: %s",strerror(errno));goto done; } break; }
  const char *name=entry->d_name;
  if(strlen(name)!=8 || strcasecmp(name+4,".mp3")) continue;
  int valid=1;for(int i=0;i<4;i++) if(name[i]<'0'||name[i]>'9') valid=0;
  if(!valid || (name[0]=='0'&&name[1]=='0') || (name[2]=='0'&&name[3]=='0')) continue;
  char path[4096];int n=snprintf(path,sizeof path,"%s/%s",music,name);
  if(n<0 || (size_t)n>=sizeof path) { snprintf(error,size,"Music path too long");goto done; }
  struct stat info;
  if(lstat(path,&info)) { snprintf(error,size,"Cannot inspect %s: %s",name,strerror(errno));goto done; }
  if(!S_ISREG(info.st_mode)) continue;
  char id[5],pid[3],pname[32],title[32];memcpy(id,name,4);id[4]=0;memcpy(pid,name,2);pid[2]=0;
  int position=(name[2]-'0')*10+name[3]-'0';
  snprintf(pname,sizeof pname,"Playlist %s",pid);snprintf(title,sizeof title,"Song %02d",position);
  sqlite3_reset(playlist);sqlite3_clear_bindings(playlist);
  if(sqlite3_bind_text(playlist,1,pid,-1,SQLITE_TRANSIENT)!=SQLITE_OK ||
     sqlite3_bind_text(playlist,2,pname,-1,SQLITE_TRANSIENT)!=SQLITE_OK || sqlite3_step(playlist)!=SQLITE_DONE) goto sql_error;
  sqlite3_reset(track);sqlite3_clear_bindings(track);
  if(sqlite3_bind_text(track,1,id,-1,SQLITE_TRANSIENT)!=SQLITE_OK ||
     sqlite3_bind_text(track,2,pid,-1,SQLITE_TRANSIENT)!=SQLITE_OK || sqlite3_bind_int(track,3,position)!=SQLITE_OK ||
     sqlite3_bind_text(track,4,path,-1,SQLITE_TRANSIENT)!=SQLITE_OK ||
     sqlite3_bind_text(track,5,title,-1,SQLITE_TRANSIENT)!=SQLITE_OK || sqlite3_step(track)!=SQLITE_DONE) goto sql_error;
  if(sqlite3_changes(db)!=1) { snprintf(error,size,"Duplicate track ID %s",id);goto done; }
 }
 if(sql(db,"COMMIT")) goto sql_error;
 transaction=0;result=0;goto done;
sql_error:
 snprintf(error,size,"SQLite scan: %s",sqlite3_errmsg(db));
done:
 sqlite3_finalize(playlist);sqlite3_finalize(track);
 if(transaction) sql(db,"ROLLBACK");
 if(db) sqlite3_close(db);
 closedir(dir);return result;
}
void library_free(MusicLibrary *l) { free(l->playlists);free(l->tracks);memset(l,0,sizeof *l); }
int library_load(MusicLibrary *l,const char *database,char *error,size_t size) {
 sqlite3 *db=NULL;sqlite3_stmt *s=NULL;MusicLibrary next={0};int result=-1;
 if(open_database(&db,database,0,error,size)) goto done;
 int version=0;
 if(sqlite3_prepare_v2(db,"PRAGMA user_version",-1,&s,NULL)!=SQLITE_OK)goto fail;
 if(sqlite3_step(s)==SQLITE_ROW)version=sqlite3_column_int(s,0);
 sqlite3_finalize(s);s=NULL;
 const char *query=version==2 ?
  "SELECT p.id,p.name,t.id,t.title,t.artist,t.album,t.file_path,m.position FROM playlists p LEFT JOIN playlist_tracks m ON m.playlist_id=p.id LEFT JOIN tracks t ON t.id=m.song_id WHERE t.available=1 OR t.id IS NULL ORDER BY p.id,m.position" :
  "SELECT p.id,p.name,t.id,t.title,t.artist,t.album,t.file_path,t.position FROM playlists p JOIN tracks t ON t.playlist_id=p.id WHERE t.available=1 ORDER BY p.id,t.position";
 if(sqlite3_prepare_v2(db,query,-1,&s,NULL)!=SQLITE_OK)goto fail;
 int rc;
 while((rc=sqlite3_step(s))==SQLITE_ROW) {
  char pid[3];copy_column(pid,sizeof pid,s,0);
  if(!next.playlist_count || strcmp(next.playlists[next.playlist_count-1].id,pid)) {
   void *p=realloc(next.playlists,(next.playlist_count+1)*sizeof *next.playlists);
   if(!p) goto memory_error;
   next.playlists=p;LibraryPlaylist *pl=&next.playlists[next.playlist_count++];memset(pl,0,sizeof *pl);
   snprintf(pl->id,sizeof pl->id,"%s",pid);copy_column(pl->name,sizeof pl->name,s,1);pl->first=next.track_count;
  }
  if(sqlite3_column_type(s,2)==SQLITE_NULL)continue;
  void *p=realloc(next.tracks,(next.track_count+1)*sizeof *next.tracks);if(!p) goto memory_error;
  next.tracks=p;LibraryTrack *t=&next.tracks[next.track_count++];memset(t,0,sizeof *t);
  copy_column(t->id,sizeof t->id,s,2);copy_column(t->title,sizeof t->title,s,3);
  copy_column(t->artist,sizeof t->artist,s,4);copy_column(t->album,sizeof t->album,s,5);copy_column(t->path,sizeof t->path,s,6);
  t->position=sqlite3_column_int(s,7);next.playlists[next.playlist_count-1].count++;
 }
 if(rc!=SQLITE_DONE) goto fail;
 library_free(l);*l=next;memset(&next,0,sizeof next);result=0;goto done;
memory_error:snprintf(error,size,"Out of memory loading library");goto done;
fail:snprintf(error,size,"SQLite load: %s",sqlite3_errmsg(db));
done:sqlite3_finalize(s);if(db) sqlite3_close(db);library_free(&next);return result;
}
