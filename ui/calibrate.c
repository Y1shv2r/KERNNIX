#define _POSIX_C_SOURCE 200809L
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

static struct fb_var_screeninfo v;
static struct fb_fix_screeninfo f;
static unsigned char *pixels;
static volatile sig_atomic_t running = 1;
static void stop(int sig) { (void)sig; running = 0; }
/* Five-bit rows, seven rows per glyph. */
static const char chars[] = "0123456789FRONTXY=:- /ABCDELSWGUHIJKMPQVZ";
static const unsigned char glyph[][7] = {
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14},{31,16,16,30,16,16,16},{30,17,17,30,20,18,17},
 {14,17,17,17,17,17,14},{17,25,25,21,19,19,17},{31,4,4,4,4,4,4},
 {17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{0,0,31,0,31,0,0},
 {0,4,4,0,4,4,0},{0,0,0,31,0,0,0},{0,0,0,0,0,0,0},
 {1,2,2,4,8,8,16},{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
 {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},
 {16,16,16,16,16,16,31},{15,16,16,14,1,1,30},{17,17,17,21,21,21,10},{14,17,16,23,17,17,14},{17,17,17,17,17,17,14}
 ,{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,18,18,12},
 {17,18,20,24,20,18,17},{17,27,21,21,17,17,17},{30,17,17,30,16,16,16},
 {14,17,17,17,21,18,13},{17,17,17,17,17,10,4},{31,1,2,4,8,16,31}
};
static uint32_t channel(unsigned n, struct fb_bitfield b) {
 return b.length ? ((n * ((1u << b.length)-1) / 255) << b.offset) : 0;
}
static void pixel(int x,int y,unsigned rgb) {
 if(x<0 || y<0 || x>=(int)v.xres || y>=(int)v.yres) return;
 uint32_t c=channel(rgb>>16 &255,v.red)|channel(rgb>>8 &255,v.green)|channel(rgb&255,v.blue);
 size_t off=(y+v.yoffset)*f.line_length+(x+v.xoffset)*(v.bits_per_pixel/8);
 if(off+v.bits_per_pixel/8<=f.smem_len) memcpy(pixels+off,&c,v.bits_per_pixel/8);
}
static void text(int x,int y,const char *s,int scale,unsigned color) {
 for(;*s;s++,x+=6*scale) {
  const char *g=strchr(chars,toupper((unsigned char)*s)); if(!g) continue;
  for(int row=0;row<7;row++) for(int col=0;col<5;col++)
   if(glyph[g-chars][row] & (16>>col))
    for(int a=0;a<scale;a++) for(int b=0;b<scale;b++) pixel(x+col*scale+a,y+row*scale+b,color);
 }
}
static void panel(int y,const char *s) {
 for(int j=y-2;j<y+9;j++) for(int i=24;i<296;i++) pixel(i,j,0xffffff);
 text(28,y,s,1,0);
}
static void background(void) {
 const unsigned colors[]={0xff0000,0x00ff00,0x0000ff};
 for(int y=0;y<240;y++) for(int x=0;x<320;x++) pixel(x,y,colors[x*3/320]);
 text(116,109,"FRONT",3,0);
 text(44,30,"R",2,0); text(153,30,"G",2,0); text(260,30,"B",2,0);
 for(int n=0;n<4;n++) {
  int cx=(n%2)?307:12,cy=(n/2)?227:12;
  for(int y=-7;y<=7;y++) for(int x=-7;x<=7;x++)
   if(x*x+y*y<=49) pixel(cx+x,cy+y,x*x+y*y<=16?0:0xffffff);
 }
 panel(64,"TOUCH TL TR BL BR IN ORDER");
}
static int open_touch(const char *path) {
 if(path) return open(path,O_RDONLY|O_CLOEXEC);
 for(int i=0;i<32;i++) {
  char p[64],name[128]={0}; snprintf(p,sizeof p,"/dev/input/event%d",i);
  int fd=open(p,O_RDONLY|O_NONBLOCK|O_CLOEXEC); if(fd<0) continue;
  if(ioctl(fd,EVIOCGNAME(sizeof name),name)>=0 && (strstr(name,"ADS7846")||strstr(name,"XPT2046"))) {
   fcntl(fd,F_SETFL,0); return fd;
  }
  close(fd);
 }
 return -1;
}
#include "player.h"
#include "player-touch.h"

int main(int argc,char **argv) {
 int fb=-1,touch,tty=-1; char path[64];
 int player = argc>1 && strcmp(argv[1],"--player")==0;
 int corners = argc>1 && strcmp(argv[1],"--corners")==0;
 int rgb = argc>1 && strcmp(argv[1],"--rgb")==0;
 int blue = argc>1 && strcmp(argv[1],"--blue")==0;
 if(blue || rgb || corners || player) { argc--; argv++; }
 if(argc>3) { fprintf(stderr,"Usage: %s [--blue|--rgb|--corners|--player] [framebuffer [touch-event]]\n",argv[0]); return 1; }
 for(int i=0;i<8;i++) {
  snprintf(path,sizeof path,"/dev/fb%d",i);
  fb=open(argc>1?argv[1]:path,O_RDWR|O_CLOEXEC); if(fb<0) { if(argc>1) break; continue; }
  if(ioctl(fb,FBIOGET_FSCREENINFO,&f)==0 && (argc>1||(strstr(f.id,"ili9341")||strstr(f.id,"st7789")))) break;
  close(fb); fb=-1;
 }
 if(fb<0 || ioctl(fb,FBIOGET_VSCREENINFO,&v)<0) { fprintf(stderr,"SPI TFT framebuffer unavailable\n"); return 1; }
 if(!((v.xres==320 && v.yres==240) || (v.xres==240 && v.yres==320)) || (v.bits_per_pixel!=16 && v.bits_per_pixel!=32) || v.red.length>8 || v.green.length>8 || v.blue.length>8) {
  fprintf(stderr,"Expected 320x240 or 240x320, 16/32 bpp; got %ux%u %u bpp\n",v.xres,v.yres,v.bits_per_pixel); close(fb); return 1;
 }
 pixels=mmap(NULL,f.smem_len,PROT_READ|PROT_WRITE,MAP_SHARED,fb,0);
 if(pixels==MAP_FAILED) { perror("mmap"); close(fb); return 1; }
 signal(SIGTERM,stop); signal(SIGINT,stop);
 tty=open("/dev/tty1",O_RDWR|O_CLOEXEC); if(tty>=0) ioctl(tty,KDSETMODE,KD_GRAPHICS);
 if(player) {
  if(v.xres!=320 || v.yres!=240) {
   fprintf(stderr,"Player requires 320x240 landscape\n");
   if(tty>=0) { ioctl(tty,KDSETMODE,KD_TEXT); close(tty); }
   munmap(pixels,f.smem_len); close(fb); return 1;
  }
  char library_error[512];
  const char *database=getenv("KERNIX_LIBRARY_DB");
  if(!database || !*database) database=KERNIX_DATABASE;
  if(library_load(&music_library,database,library_error,sizeof library_error))
   fprintf(stderr,"Library unavailable: %s\n",library_error);
  unsigned char *mapped=pixels;
  unsigned char *canvas=calloc(1,f.smem_len);
  int failed=canvas==NULL;
  if(!canvas) perror("player allocation");
  if(audio_start(&audio_backend,&music_library)) fprintf(stderr,"Cannot start audio worker\n");
  struct player_touch input; player_touch_init(&input,argc>2?argv[2]:NULL);
  printf("SQLite library: %zu playlists, %zu tracks; audio worker enabled\n",music_library.playlist_count,music_library.track_count); fflush(stdout);
  unsigned frame=0;
  player_activity(player_now_ms());
  while(running && canvas) {
   player_audio_sync();
   player_touch_poll(&input);
   player_idle_tick(player_now_ms());
   pixels=canvas; player_render(frame++);
   size_t done=0;
   while(done<f.smem_len && running) {
    ssize_t n=pwrite(fb,canvas+done,f.smem_len-done,(off_t)done);
    if(n<0 && errno==EINTR) continue;
    if(n<=0) { perror("player write"); failed=1; running=0; break; }
    done+=(size_t)n;
   }
   struct timespec delay={0,100000000}; nanosleep(&delay,NULL);
  }
  if(input.fd>=0) close(input.fd);
  audio_shutdown(&audio_backend);
  library_free(&music_library);
  free(canvas); pixels=mapped;
  if(tty>=0) { ioctl(tty,KDSETMODE,KD_TEXT); close(tty); }
  munmap(mapped,f.smem_len); close(fb); return failed;
 }
 if(blue || rgb || corners) {
  const unsigned colors[]={0xff0000,0x00ff00,0x0000ff};
  for(unsigned y=0;y<v.yres;y++) for(unsigned x=0;x<v.xres;x++)
   pixel(x,y,rgb ? colors[x*3/v.xres] : 0x0000ff);
  if(rgb) {
   const char *labels[]={"RED","GREEN","BLUE"};
   for(unsigned band=0;band<3;band++) {
    int centre=(int)((2*band+1)*v.xres/6);
    text(centre-(int)strlen(labels[band])*6,(int)v.yres/2-7,labels[band],2,0);
   }
  }
  if(corners) {
   /* Dark pink background; inset dot centres keep the circles visible. */
   for(unsigned y=0;y<v.yres;y++) for(unsigned x=0;x<v.xres;x++) pixel(x,y,0xc2185b);
   const unsigned dots[]={0xffff00,0xff0000,0x000000,0x0000ff};
   for(unsigned n=0;n<4;n++) {
    int cx=n%2 ? (int)v.xres-17 : 16;
    int cy=n/2 ? (int)v.yres-17 : 16;
    for(int dy=-10;dy<=10;dy++) for(int dx=-10;dx<=10;dx++)
     if(dx*dx+dy*dy<=100) pixel(cx+dx,cy+dy,dots[n]);
   }
  }
  /* Use a separate full-frame snapshot: write() explicitly dirties the
   * framebuffer instead of relying only on mmap deferred-I/O tracking. */
  unsigned char *frame=malloc(f.smem_len);
  if(!frame) { perror("frame allocation"); running=0; }
  else memcpy(frame,pixels,f.smem_len);
  puts(corners ? "Dark pink corner test" : rgb ? "RGB vertical bands: full-frame write every second" : "Solid blue display test: full-frame write every second"); fflush(stdout);
  int write_failed=frame==NULL;
  while(running) {
   size_t done=0;
   while(done<f.smem_len && running) {
    ssize_t n=pwrite(fb,frame+done,f.smem_len-done,(off_t)done);
    if(n<0 && errno==EINTR) continue;
    if(n<=0) { perror("full framebuffer write"); write_failed=1; running=0; break; }
    done+=(size_t)n;
   }
   if(running) sleep(1);
  }
  free(frame);
  if(tty>=0) { ioctl(tty,KDSETMODE,KD_TEXT); close(tty); }
  munmap(pixels,f.smem_len); close(fb); return write_failed;
 }
 background(); touch=open_touch(argc>2?argv[2]:NULL);
 int status=0;
 if(touch<0) { panel(150,"NO TOUCH DEVICE - CHECK WIRING"); fprintf(stderr,"Touch device unavailable\n"); status=1; }
 else {
  struct input_absinfo ax,ay;
  if(ioctl(touch,EVIOCGABS(ABS_X),&ax)<0 || ioctl(touch,EVIOCGABS(ABS_Y),&ay)<0) { perror("touch axes"); status=1; }
  else {
   char line[80]; int x=ax.value,y=ay.value,down=0,have_x=0,have_y=0,slot=0,dropped=0;
   const char *labels[]={"TL","TR","BL","BR"};
   fprintf(stdout,"RAW ranges X=%d..%d Y=%d..%d\n",ax.minimum,ax.maximum,ay.minimum,ay.maximum); fflush(stdout);
   while(running) {
    struct input_event ev; ssize_t n=read(touch,&ev,sizeof ev);
    if(n<0 && errno==EINTR) continue;
    if(n!=sizeof ev) { status=1; break; }
    if(ev.type==EV_SYN && ev.code==SYN_DROPPED) { dropped=1; continue; }
    if(dropped) {
     if(ev.type==EV_SYN && ev.code==SYN_REPORT) { dropped=0; down=0; have_x=have_y=0; }
     continue;
    }
    if(ev.type==EV_ABS && ev.code==ABS_X) { x=ev.value; have_x=1; }
    if(ev.type==EV_ABS && ev.code==ABS_Y) { y=ev.value; have_y=1; }
    if(ev.type==EV_KEY && ev.code==BTN_TOUCH) {
     down=ev.value;
     if(down) {
      if(ioctl(touch,EVIOCGABS(ABS_X),&ax)==0) { x=ax.value; have_x=1; }
      if(ioctl(touch,EVIOCGABS(ABS_Y),&ay)==0) { y=ay.value; have_y=1; }
     }
    }
    if(ev.type==EV_SYN && ev.code==SYN_REPORT && down && have_x && have_y) {
     snprintf(line,sizeof line,"%s RAW X=%d Y=%d",labels[slot],x,y); panel(150+slot*16,line);
    }
    if(ev.type==EV_SYN && ev.code==SYN_REPORT && !down && have_x && have_y) {
     snprintf(line,sizeof line,"%s RAW X=%d Y=%d",labels[slot],x,y); panel(150+slot*16,line);
     puts(line); fflush(stdout); slot=(slot+1)%4; have_x=have_y=0;
    }
   }
  }
  close(touch);
 }
 if(tty>=0) { ioctl(tty,KDSETMODE,KD_TEXT); close(tty); }
 munmap(pixels,f.smem_len); close(fb); return status;
}
