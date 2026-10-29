#ifndef KERNIX_THEME_H
#define KERNIX_THEME_H
/* Ukiyo palette. Drawing only: no layout, input or audio state lives here. */
#define THEME_INK 0x050b0e
#define THEME_TEXT 0xf1efe6
#define THEME_MUTED 0xa8afa6
#define THEME_CREAM 0xd1d98b
#define THEME_BLUE 0xd5dfa0
#define THEME_EDGE 0x3c4944
#define THEME_SELECTED_EDGE 0xb8c771
#define THEME_PAIRED_EDGE 0xb5d49b
#define THEME_OUTPUT_EDGE 0xa7b764
#define THEME_MODE_EDGE 0x697741
#define THEME_MODE_TEXT 0xd0db93
#define THEME_PLAY_TOP 0xd8dfa1
#define THEME_PLAY_BOTTOM 0xaeba6a
#define THEME_PLAY_EDGE 0xecedc1
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
#include "../assets/ukiyo-wave.h"
static void theme_artwork(unsigned frame) {
 for(int y=0;y<144;y++)for(int x=0;x<144;x++){
  if((y==0||y==143)&&(x<2||x>141))continue;
  unsigned raw=theme_wave[y*144+x];
  unsigned color=(((raw>>11)&31)*255/31)<<16 | (((raw>>5)&63)*255/63)<<8 | (raw&31)*255/31;
  pixel(9+x,9+y,color);
 }
 /* Tiny foam glints animate inside the existing artwork panel. */
 for(int i=0;i<4;i++){
  int x=58+i*19+(int)(frame/6%3),y=132+i*4;
  boxfill(x,y,3,1,0xe8dfbf);
 }
}
#endif
