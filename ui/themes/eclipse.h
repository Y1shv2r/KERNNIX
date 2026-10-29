#ifndef KERNIX_THEME_H
#define KERNIX_THEME_H
/* Eclipse palette. Drawing only: no layout, input or audio state lives here. */
#define THEME_INK 0x050b0e
#define THEME_TEXT 0xf1efe6
#define THEME_MUTED 0xa2adba
#define THEME_CREAM 0xbdd8fb
#define THEME_BLUE 0xb9d8ff
#define THEME_EDGE 0x394751
#define THEME_SELECTED_EDGE 0x9bbbdc
#define THEME_PAIRED_EDGE 0xb1d1bf
#define THEME_OUTPUT_EDGE 0x90b2da
#define THEME_MODE_EDGE 0x657f9f
#define THEME_MODE_TEXT 0xbad6f6
#define THEME_PLAY_TOP 0xc9e0ff
#define THEME_PLAY_BOTTOM 0x93b6e6
#define THEME_PLAY_EDGE 0xd8e9ff
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
#include "../assets/eclipse-marble.h"
static void theme_artwork(unsigned frame) {
 for(int y=0;y<144;y++)for(int x=0;x<144;x++){
  if((y==0||y==143)&&(x<2||x>141))continue;
  unsigned raw=theme_marble[y*144+x];
  unsigned color=(((raw>>11)&31)*255/31)<<16 | (((raw>>5)&63)*255/63)<<8 | (raw&31)*255/31;
  pixel(9+x,9+y,color);
 }
 /* Restrained starlight movement inside the unchanged artwork panel. */
 for(int i=0;i<3;i++){
  int x=24+i*9,y=18+i*5;
  unsigned tone=((frame/12+(unsigned)i)%3)==0?0xb9c8d5:0x34434e;
  pixel(x,y,tone);
 }
}
#endif
