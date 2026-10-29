/* Raw mapping followed by a correction fitted to measured screen corners. */
static int touch_option(const char *key,int fallback) {
 const char *s=getenv(key); if(!s || !*s) return fallback;
 char *end; long value=strtol(s,&end,10);
 return *end || value < -65535 || value > 65535 ? fallback : (int)value;
}
static int scaled_axis(int raw,int low,int high,int extent) {
 if(high<=low) return 0;
 if(raw<low) raw=low;
 if(raw>high) raw=high;
 return (int)((int64_t)(raw-low)*(extent-1)/(high-low));
}
static int corrected_axis(int value,int scale,int offset,int extent) {
 int corrected=(value*scale+offset)/100;
 if(corrected<0) return 0;
 if(corrected>=extent) return extent-1;
 return corrected;
}
static void correct_screen_touch(int *x,int *y) {
 *x=corrected_axis(*x,touch_option("KERNIX_TOUCH_SCREEN_X_SCALE",100),touch_option("KERNIX_TOUCH_SCREEN_X_OFFSET",0),320);
 *y=corrected_axis(*y,touch_option("KERNIX_TOUCH_SCREEN_Y_SCALE",100),touch_option("KERNIX_TOUCH_SCREEN_Y_OFFSET",0),240);
}
struct player_touch { int fd,x,y,down,active,dropped; struct input_absinfo ax,ay; };
static void player_touch_init(struct player_touch *t,const char *path) {
 memset(t,0,sizeof *t); t->fd=open_touch(path);
 if(t->fd<0) { perror("Player touch device"); return; }
 if(ioctl(t->fd,EVIOCGABS(ABS_X),&t->ax)<0 || ioctl(t->fd,EVIOCGABS(ABS_Y),&t->ay)<0) {
  perror("Player touch axes"); close(t->fd); t->fd=-1; return;
 }
 fcntl(t->fd,F_SETFL,O_NONBLOCK);
}
static void player_touch_poll(struct player_touch *t) {
 if(t->fd<0) return;
 for(int i=0;i<256;i++) {
  struct input_event e; ssize_t n=read(t->fd,&e,sizeof e);
  if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) return;
  if(n<0 && errno==EINTR) continue;
  if(n!=sizeof e) { perror("Player touch read"); close(t->fd);t->fd=-1;return; }
  if(e.type==EV_SYN && e.code==SYN_DROPPED) { t->dropped=1;t->active=0;t->down=0;continue; }
  if(t->dropped) {
   if(e.type==EV_SYN && e.code==SYN_REPORT) t->dropped=0;
   continue;
  }
  if(e.type==EV_ABS && e.code==ABS_X) t->x=e.value;
  if(e.type==EV_ABS && e.code==ABS_Y) t->y=e.value;
  if(e.type==EV_KEY && e.code==BTN_TOUCH) {
   t->down=e.value;
   if(t->down) {
    if(ioctl(t->fd,EVIOCGABS(ABS_X),&t->ax)==0) t->x=t->ax.value;
    if(ioctl(t->fd,EVIOCGABS(ABS_Y),&t->ay)==0) t->y=t->ay.value;
    t->active=!player_activity(player_now_ms());
   }
  }
  if(e.type==EV_SYN && e.code==SYN_REPORT && t->down) player_activity(player_now_ms());
  if(e.type==EV_SYN && e.code==SYN_REPORT && !t->down && t->active) {
   player_activity(player_now_ms());
   int x=scaled_axis(t->x,touch_option("KERNIX_TOUCH_XMIN",t->ax.minimum),touch_option("KERNIX_TOUCH_XMAX",t->ax.maximum),touch_option("KERNIX_TOUCH_SWAP",1)?240:320);
   int y=scaled_axis(t->y,touch_option("KERNIX_TOUCH_YMIN",t->ay.minimum),touch_option("KERNIX_TOUCH_YMAX",t->ay.maximum),touch_option("KERNIX_TOUCH_SWAP",1)?320:240);
   if(touch_option("KERNIX_TOUCH_SWAP",1)) { int tmp=x;x=y;y=tmp; }
   if(touch_option("KERNIX_TOUCH_INVERT_X",0)) x=319-x;
   if(touch_option("KERNIX_TOUCH_INVERT_Y",1)) y=239-y;
   int base_x=x,base_y=y;
   correct_screen_touch(&x,&y);
   printf("Touch raw=%d,%d base=%d,%d screen=%d,%d\n",t->x,t->y,base_x,base_y,x,y);fflush(stdout);
   player_click(x,y); t->active=0;
  }
 }
}

