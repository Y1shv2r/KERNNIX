#ifndef KERNIX_THEME_H
#define KERNIX_THEME_H
/* Moonlit palette. Drawing only: no layout, input or audio state lives here. */
#define THEME_INK 0x050b0e
#define THEME_TEXT 0xf1efe6
#define THEME_MUTED 0x9daab3
#define THEME_CREAM 0xf3dfb6
#define THEME_BLUE 0x91b9f6
#define THEME_EDGE 0x36474f
#define THEME_SELECTED_EDGE 0xb7a17e
#define THEME_PAIRED_EDGE 0x8ac9a1
#define THEME_OUTPUT_EDGE 0x577eb0
#define THEME_MODE_EDGE 0x37534b
#define THEME_MODE_TEXT 0xb6d2bc
#define THEME_PLAY_TOP 0xf6e7c9
#define THEME_PLAY_BOTTOM 0xd9c49e
#define THEME_PLAY_EDGE 0xffedcd
#define THEME_PLAY_ICON 0x172029
static unsigned theme_mix(unsigned a,unsigned b,int amount) {
 unsigned result=0;
 for(int shift=0;shift<=16;shift+=8) {
  int value=((int)((a>>shift)&255)*(255-amount)+(int)((b>>shift)&255)*amount)/255;
  result|=(unsigned)value<<shift;
 }
 return result;
}
#include "../assets/fonts/theme-font.h"
static unsigned theme_sample(int x,int y) {
 if(x<0||y<0||x>=(int)v.xres||y>=(int)v.yres)return THEME_INK;
 size_t off=(y+v.yoffset)*f.line_length+(x+v.xoffset)*(v.bits_per_pixel/8);
 if(off+v.bits_per_pixel/8>f.smem_len)return THEME_INK;
 unsigned raw=0,result=0;memcpy(&raw,pixels+off,v.bits_per_pixel/8);
 struct fb_bitfield fields[]={v.blue,v.green,v.red};
 for(int i=0;i<3;i++)if(fields[i].length){
  unsigned mask=(1u<<fields[i].length)-1;
  result|=(((raw>>fields[i].offset)&mask)*255/mask)<<(i*8);
 }
 return result;
}
static void theme_text(int x,int y,const char *text_value,int scale,unsigned color) {
 int width=scale==1?6:12,height=scale==1?10:16;
 for(;*text_value;text_value++,x+=6*scale){
  unsigned code=(unsigned char)toupper((unsigned char)*text_value);
  if(code<32||code>126)continue;
  const unsigned char *mask=scale==1?theme_font_small[code-32]:theme_font_large[code-32];
  for(int row=0;row<height;row++)for(int col=0;col<width;col++){
   int alpha=mask[row*width+col];
   if(alpha)pixel(x+col,y+row,alpha==255?color:theme_mix(theme_sample(x+col,y+row),color,alpha));
  }
 }
}
static int theme_inset(int row,int height) {
 if(row==0||row==height-1)return 3;
 if(row==1||row==height-2)return 1;
 return 0;
}
static void theme_background(void) {
 for(int y=0;y<240;y++)boxfill(0,y,320,1,theme_mix(0x0b1419,0x020608,y*255/239));
}
static void theme_card(int x,int y,int w,int h,unsigned top,unsigned bottom,unsigned edge) {
 for(int row=0;row<h;row++) {
  int inset=theme_inset(row,h);
  unsigned fill=theme_mix(top,bottom,row*255/(h>1?h-1:1));
  boxfill(x+inset,y+row,w-2*inset,1,fill);
  if(row==0||row==h-1)boxfill(x+inset,y+row,w-2*inset,1,edge);
  else {pixel(x+inset,y+row,edge);pixel(x+w-1-inset,y+row,edge);}
 }
 /* A quiet top highlight reads as an inset hardware control at TFT scale. */
 if(h>8)boxfill(x+4,y+1,w-8,1,theme_mix(top,edge,65));
}
static int theme_ridge(int x,const int *xs,const int *ys,int count) {
 for(int i=1;i<count;i++)if(x<=xs[i])
  return ys[i-1]+(ys[i]-ys[i-1])*(x-xs[i-1])/(xs[i]-xs[i-1]);
 return ys[count-1];
}
static void theme_artwork(unsigned frame) {
 /* Precompute the detailed artwork once; only a few water glints animate. */
 static unsigned art[144*144];static int ready;
 if(!ready) {
  const int xs[]={0,22,43,65,86,113,143};
  const int distant[]={93,79,91,60,85,71,94};
  const int near[]={111,95,104,87,110,97,108};
  for(int x=0;x<144;x++)for(int y=0;y<144;y++) {
   unsigned color=theme_mix(0x101b30,0x654855,y<99?y*255/99:255);
   int dx=x-104,dy=y-34,d2=dx*dx+dy*dy;
   if(d2<27*27)color=theme_mix(color,0xbfae94,(27*27-d2)*35/(27*27));
   if(d2<=17*17) {
    color=theme_mix(0xf8ead0,0xcbbca3,(x+2*y)%9*5);
    int crater=(x*17+y*31+(x*y)%23)%71;
    if(crater<8)color=theme_mix(color,0x9d948d,55);
    if(dx<-9)color=theme_mix(color,0x8e92a1,50);
   }
   int ridge=theme_ridge(x,xs,distant,7);
   if(y>=ridge) {
    color=theme_mix(0x273044,0x111c29,(y-ridge)*5>255?255:(y-ridge)*5);
    if(x<65 && y-ridge<2)color=0x646275;
   }
   if(y>=theme_ridge(x,xs,near,7))color=0x09141f;
   if(y>=113) {
    color=theme_mix(0x101f2a,0x030a10,(y-113)*255/30);
    int width=(y-112)/2+2;
    if(abs(x-102)<width && y%3==0)color=theme_mix(color,0xc4b192,90-(y-113));
    if((x*11+y*7)%67==0 && y%3==0)color=0x293a46;
   }
   art[y*144+x]=color;
  }
  unsigned random=23147;
  for(int i=0;i<24;i++) {
   random=random*1664525u+1013904223u;int x=(int)((random>>16)%140)+2;
   random=random*1664525u+1013904223u;int y=(int)((random>>16)%55)+3;
   if((x-104)*(x-104)+(y-34)*(y-34)>25*25)art[y*144+x]=i%3?0x8895aa:0xe2dac5;
  }
  ready=1;
 }
 for(int y=0;y<144;y++)for(int x=0;x<144;x++) {
  if((y==0||y==143)&&(x<2||x>141))continue;
  pixel(9+x,9+y,art[y*144+x]);
 }
 for(int i=0;i<5;i++) {
  int y=124+i*5,x=112-(i+1)*2+(int)((frame/5+(unsigned)i)%3)-1;
  boxfill(x,y,4+i*2,1,theme_mix(0x1c2b35,THEME_CREAM,80-i*9));
 }
}
#endif
