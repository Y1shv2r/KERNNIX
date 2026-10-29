#define _POSIX_C_SOURCE 200809L
#include <linux/gpio.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void fail(const char *s) { perror(s); exit(1); }
int main(void) {
 int chip=open("/dev/gpiochip0",O_RDWR);
 if(chip<0) fail("gpiochip0");
 struct gpiochip_info ci={0};
 if(ioctl(chip,GPIO_GET_CHIPINFO_IOCTL,&ci)<0) fail("GPIO chip info");
 if(!strstr(ci.label,"bcm2835")) { fprintf(stderr,"Unexpected GPIO chip: %s; refusing GPIO access\n",ci.label); return 1; }
 struct gpiohandle_request req={0};
 req.lines=2; req.lineoffsets[0]=25; req.lineoffsets[1]=24;
 req.flags=GPIOHANDLE_REQUEST_OUTPUT; req.default_values[0]=1; req.default_values[1]=1;
 strcpy(req.consumer_label,"kernix-lcd-probe");
 if(ioctl(chip,GPIO_GET_LINEHANDLE_IOCTL,&req)<0) fail("LCD GPIO request");
 int spi=open("/dev/spidev0.0",O_RDWR);
 if(spi<0) fail("spidev0.0");
 uint8_t mode=0,bits=8; uint32_t speed=100000;
 if(ioctl(spi,SPI_IOC_WR_MODE,&mode)<0 || ioctl(spi,SPI_IOC_WR_BITS_PER_WORD,&bits)<0 || ioctl(spi,SPI_IOC_WR_MAX_SPEED_HZ,&speed)<0) fail("SPI setup");
 const uint8_t commands[]={0x04,0x09,0x0b,0x0c,0xd3,0xda,0xdb,0xdc};
 const char *names[]={"DISPLAY_ID","STATUS","MADCTL","PIXEL_FORMAT","ID4","ID1","ID2","ID3"};
 puts("LCD register readback at 100 kHz; initial bytes can include dummy bits.");
 for(unsigned i=0;i<sizeof commands;i++) {
  struct gpiohandle_data dc={0}; dc.values[1]=1;
  if(ioctl(req.fd,GPIOHANDLE_SET_LINE_VALUES_IOCTL,&dc)<0) fail("DC low");
  /* Keep CS asserted across the command and DC transition. */
  struct spi_ioc_transfer command={0};
  command.tx_buf=(uintptr_t)&commands[i]; command.len=1;
  command.speed_hz=speed; command.bits_per_word=8; command.cs_change=1;
  if(ioctl(spi,SPI_IOC_MESSAGE(1),&command)<0) fail("SPI command");
  dc.values[0]=1;
  if(ioctl(req.fd,GPIOHANDLE_SET_LINE_VALUES_IOCTL,&dc)<0) fail("DC high");
  uint8_t tx[8]={0},rx[8]={0};
  struct spi_ioc_transfer tr={0};
  tr.tx_buf=(uintptr_t)tx; tr.rx_buf=(uintptr_t)rx; tr.len=sizeof rx; tr.speed_hz=speed; tr.bits_per_word=8;
  if(ioctl(spi,SPI_IOC_MESSAGE(1),&tr)<0) fail("SPI read");
  printf("%s (0x%02x):",names[i],commands[i]);
  for(unsigned j=0;j<sizeof rx;j++) printf(" %02x",rx[j]);
  putchar('\n');
 }
 close(spi); close(req.fd); close(chip); return 0;
}
