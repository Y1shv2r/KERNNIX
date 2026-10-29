#ifndef KERNIX_THEME_H
#define KERNIX_THEME_H
/* Crimson palette. Drawing only: no layout, input or audio state lives here. */
#define THEME_INK 0x050b0e
#define THEME_TEXT 0xf1efe6
#define THEME_MUTED 0xb4a6a6
#define THEME_CREAM 0xff424a
#define THEME_BLUE 0xff6666
#define THEME_EDGE 0x4a3338
#define THEME_SELECTED_EDGE 0xd84048
#define THEME_PAIRED_EDGE 0xb1d1bf
#define THEME_OUTPUT_EDGE 0xd93943
#define THEME_MODE_EDGE 0x7e3037
#define THEME_MODE_TEXT 0xedb9ae
#define THEME_PLAY_TOP 0xff4b4c
#define THEME_PLAY_BOTTOM 0xd02031
#define THEME_PLAY_EDGE 0xff7c6d
#define THEME_PLAY_ICON 0xfff0de
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
 for(int y=0;y<240;y++)boxfill(0,y,320,1,theme_mix(0x130c0f,0x040507,y*255/239));
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
#include "../assets/crimson-blossom.h"
static void theme_artwork(unsigned frame) {
 for(int y=0;y<144;y++)for(int x=0;x<144;x++){
  if((y==0||y==143)&&(x<2||x>141))continue;
  unsigned raw=theme_blossom[y*144+x];
  unsigned color=(((raw>>11)&31)*255/31)<<16 | (((raw>>5)&63)*255/63)<<8 | (raw&31)*255/31;
  pixel(9+x,9+y,color);
 }
 /* Subtle drifting petals inside the unchanged artwork panel. */
 for(int i=0;i<3;i++){
  int x=28+i*36+(int)(frame/9%3),y=135+i*4;
  unsigned tone=((frame/12+(unsigned)i)%3)==0?0xea5347:0x8e3030;
  pixel(x,y,tone);
 }
}
#endif
