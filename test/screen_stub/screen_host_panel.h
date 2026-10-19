#pragma once
#include <Adafruit_GFX.h>
#include "SPI.h"
#include <vector>
#include <fstream>
class DisplayPanel : public Adafruit_GFX {
 public:
  static constexpr uint8_t DefaultRotation=0;
  static constexpr uint32_t SpiFrequency=20000000;
  static DisplayPanel *instance;
  unsigned clipped=0;
  DisplayPanel():Adafruit_GFX(320,480),pixels(320*480,0){instance=this;}
  void begin(uint32_t){}
  void drawPixel(int16_t x,int16_t y,uint16_t color)override{
    if(x<0||y<0||x>=_width||y>=_height){++clipped;return;}pixels[y*_width+x]=color;
  }
  void save(const char *path){
    std::ofstream out(path,std::ios::binary);
    const uint32_t stride=(_width*3+3)&~3u,size=54+stride*_height;
    uint8_t header[54]={0x42,0x4d};
    auto put=[&](int off,uint32_t v){for(int i=0;i<4;++i)header[off+i]=(v>>(i*8))&255;};
    put(2,size);put(10,54);put(14,40);put(18,_width);put(22,_height);header[26]=1;header[28]=24;
    out.write(reinterpret_cast<char *>(header),54);
    std::vector<uint8_t> line(stride,0);
    for(int y=_height-1;y>=0;--y){for(int x=0;x<_width;++x){const uint16_t c=pixels[y*_width+x];
      line[x*3]=(c&31)*255/31;line[x*3+1]=((c>>5)&63)*255/63;line[x*3+2]=((c>>11)&31)*255/31;}
      out.write(reinterpret_cast<char *>(line.data()),line.size());}
  }
 private:
  std::vector<uint16_t> pixels;
};
