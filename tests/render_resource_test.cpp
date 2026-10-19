#define PVZ_RENDER_TRACE_TEST
#include "../src/native/render_trace.cpp"
#include <iostream>
int main() {
  using pvz::guest::Write;
  std::vector<uint8_t> bytes(0x4000);
  auto* b=bytes.data();
  auto require=[](bool p) {if(!p) throw std::runtime_error("render resource identity");};
  require(TraceClockSeconds(0)==0 && TraceClockSeconds(99)==0);
  require(TraceClockSeconds(100)==1 && TraceClockSeconds(199)==1);
  require(TraceClockSeconds(200)==2 && TraceClockSeconds(6000)==60);
  auto string=[&](uint32_t at,const char* text) {
    auto size=uint32_t(std::strlen(text));require(size<16);
    std::memset(b+at,0,28);std::memcpy(b+at+4,text,size);
    Write(b,at+20,size);Write(b,at+24,15);
  };
  for(auto font:{0x1000u,0x1100u}) {
    Write(b,font,0x8207A134);Write(b,font+20,font+0x1000);
    string(font+0x1000+244,"font/A");Write(b,font+24,12);
  }
  require(FontKey(b,0x1000)==FontKey(b,0x1100));
  Write(b,0x1118,13);require(FontKey(b,0x1000)!=FontKey(b,0x1100));
  Write(b,0x1118,12);string(0x2100+244,"font/B");
  require(FontKey(b,0x1000)!=FontKey(b,0x1100));
  for(auto image:{0x3000u,0x3100u}) {
    Write(b,image,0x82068DE4);string(image+8,"image/A");Write(b,image+36,128);
  }
  require(ImageKey(b,0x3000)==ImageKey(b,0x3100));
  Write(b,0x3124,129);require(ImageKey(b,0x3000)!=ImageKey(b,0x3100));
  Write(b,0x3124,128);string(0x3108,"image/B");
  require(ImageKey(b,0x3000)!=ImageKey(b,0x3100));
  string(0x3008,"");string(0x3108,"");
  require(ImageKey(b,0x3000)!=ImageKey(b,0x3100));
  std::cout<<"render identities: relocation equal, different content/state unequal, unknowns retained\n";
}
