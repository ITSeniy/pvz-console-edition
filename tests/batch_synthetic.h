// Shared isolated domains, actual Xbox constants and sparse guest address space.
#pragma once
#define PVZ_NATIVE_REPLACEMENT_TEST
#define PVZ_BATCH_SYNTHETIC_TEST
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "pvz_pch.h"
#include "native/replacement.h"
#include "guest_memory.h"
#include "xbox/Gamepad.h"
#include "xbox/BaseGamepadControls.h"
#include "xbox/Widget.h"
#include "xbox/WidgetContainer.h"
#include "xbox/ScrollbarWidget.h"
#include <cmath>
#include <initializer_list>
#include <fstream>
#include <iostream>
#include <functional>
namespace pvz::native {using RegisterOutput=Output<PPCContext,PPCRegister>;}
namespace {
uint64_t Random(uint64_t& s){s^=s<<13;s^=s>>7;s^=s<<17;return s;}
constexpr uint64_t Words[]{0,1,0x7fffffff,0x80000000,0xffffffff,0x100000000,0x7fffffffffffffff,0x8000000000000000,~uint64_t{}};
constexpr uint64_t Floats[]{0,0x8000000000000000,0x3FF0000000000000,0xBFF0000000000000,
  0x3FEFFFFFFFFFFFFF,0x3FF0000000000001,0x7FF0000000000000,0xFFF0000000000000,
  0x7FF8000000001234,0xFFF8000000005678,0x7FF0000000000001,1,0x0010000000000000,
  0x47F0000000000000};
using pvz::native::SyntheticInputs;
const SyntheticInputs<PPCContext> IdentityInputs{"r3.u64 boundaries and 1024 xorshift64 seed=42",9,1024,42,
  [](size_t i,PPCContext& c,uint8_t*,uint64_t& s){c.r3.u64=i<9?Words[i]:Random(s);}};
const SyntheticInputs<PPCContext> ConnectedInputs{"self=1000; +C all 256 bytes and 1024 seeded",256,1024,42,
  [](size_t i,PPCContext& c,uint8_t* b,uint64_t& s){c.r3.u64=0x1000;b[0x100C]=uint8_t(i<256?i:Random(s));}};
const auto FloatZeroInputs=IdentityInputs;
const SyntheticInputs<PPCContext> AxisInputs{"self=1000; SP=7000; signed 32-bit axes incl extrema, high register bits, 1024 seeded; entry.SP-16 eight-byte store",81,1024,42,
  [](size_t i,PPCContext& c,uint8_t* b,uint64_t& s){c.r3.u64=0x1000;c.r1.u64=0x7000;
    pvz::guest::Write(b,0x1010,uint32_t(i<81?Words[i%9]:Random(s)));
    pvz::guest::Write(b,0x1014,uint32_t(i<81?Words[i/9]:Random(s)));}};
const SyntheticInputs<PPCContext> VibrationInputs{"self=1000; f1/f2/f3 double bits Cartesian bounds incl +/-0,1+/-ulp, infinities, NaNs, subnormals; 1024 seeded",14*14*14,1024,42,
  [](size_t i,PPCContext& c,uint8_t*,uint64_t& s){c.r3.u64=0x1000;
    c.f1.u64=i<2744?Floats[i%14]:Random(s);c.f2.u64=i<2744?Floats[(i/14)%14]:Random(s);c.f3.u64=i<2744?Floats[i/196]:Random(s);}};
const SyntheticInputs<PPCContext> CountInputs{"self=1000; +38 word wrap boundaries and 1024 seeded",9,1024,42,
  [](size_t i,PPCContext& c,uint8_t* b,uint64_t& s){c.r3.u64=0x1000;pvz::guest::Write(b,0x1038,uint32_t(i<9?Words[i]:Random(s)));}};
const SyntheticInputs<PPCContext> Field64Inputs{"self=1000; r4 full 64-bit boundaries and 1024 xorshift64 seed=42; word self+64, high half truncated",9,1024,42,
  [](size_t i,PPCContext& c,uint8_t* b,uint64_t& s){c.r3.u64=0x1000;c.r4.u64=i<9?Words[i]:Random(s);pvz::guest::Write(b,0x1064,~c.r4.u32);}};
const SyntheticInputs<PPCContext> RectInputs{"source self=1000; destination distinct, equal, partial +/-4/+8 overlap; all source words loaded before stores; 1024 seeded",7*9,1024,42,
  [](size_t i,PPCContext& c,uint8_t* b,uint64_t& s){constexpr uint32_t aliases[]{0x2000,0x1040,0x103C,0x1044,0x1048,0x104C,0x1038};
    c.r4.u64=0x1000;c.r3.u64=aliases[(i<63?i:Random(s))%7];
    for(uint32_t j=0;j<4;++j)pvz::guest::Write(b,0x1040+j*4,uint32_t(i<63?Words[(i/7+j)%9]:Random(s)));}};
struct Test{const char* address;const char* domain;std::function<pvz::native::SyntheticResult()> run;};
auto& Tests(){static std::vector<Test> v;return v;}
struct AddTest{AddTest(Test t){Tests().push_back(std::move(t));}};
// Original global loads retain their real Xbox addresses. Reserve 2.2 GiB of
// virtual address space per independent buffer, committing only 32 KiB of test
// objects and 128 KiB of read-only Xbox image data. No rewritten reference loads.
struct Sparse {
  uint8_t* b=static_cast<uint8_t*>(VirtualAlloc(nullptr,0x83000000ULL,MEM_RESERVE,PAGE_NOACCESS));
  Sparse(){if(!b||!VirtualAlloc(b,0x8000,MEM_COMMIT,PAGE_READWRITE)||
      !VirtualAlloc(b+0x82000000,0x20000,MEM_COMMIT,PAGE_READWRITE))throw std::bad_alloc();}
  ~Sparse(){if(b)VirtualFree(b,0,MEM_RELEASE);}
};
std::vector<uint8_t> constants;
template<class Outputs,class Resolve,class Native,class Original>
pvz::native::SyntheticResult VerifySparseSynthetic(const SyntheticInputs<PPCContext>& inputs,
    const Outputs& outputs,Resolve resolve,Native native,Original original){
  Sparse a,b;pvz::native::SyntheticResult result;uint64_t seed=inputs.seed;
  for(size_t i=0;i<inputs.boundaries+inputs.random;++i){
    std::memset(a.b,0xCD,0x8000);std::memcpy(a.b+0x82000000,constants.data(),constants.size());PPCContext c{};
    inputs.generate(i,c,a.b,seed);const auto ranges=resolve(c);
    for(auto r:ranges)if(!r.size||uint64_t(r.address)+r.size>0x8000)throw std::runtime_error("bad synthetic range");
    std::memcpy(b.b,a.b,0x8000);std::memcpy(b.b+0x82000000,a.b+0x82000000,constants.size());PPCContext x=c,y=c;
    native(x,a.b);original(y,b.b);
    auto equal=[&]{for(auto& o:outputs)if(o.Read(x)!=o.Read(y))return false;
      for(auto r:ranges)if(std::memcmp(a.b+r.address,b.b+r.address,r.size))return false;return true;};
    ++result.cases;if(!equal()){
      ++result.errors;if(result.errors<=3)std::cerr<<"mismatch case "<<i<<" f1 "<<std::hex<<x.f1.u64<<" / "<<y.f1.u64<<std::dec<<"\n";
    }
    pvz::native::InjectError(x,a.b,outputs,ranges);result.negative_errors+=!equal();
  }return result;
}
int Run(int argc,char** argv,size_t expected){
  std::ifstream image(".cache/pvz-image.bin",std::ios::binary);constants.resize(0x20000);
  if(!image.read(reinterpret_cast<char*>(constants.data()),constants.size()))throw std::runtime_error("Xbox constants missing");
  std::ofstream file;if(argc>1)file.open(argv[1]);auto& out=argc>1?file:std::cout;
  bool pass=Tests().size()==expected;out<<"{\"seed\":42,\"functions\":[";bool first=true;
  for(const auto& t:Tests()){std::cerr<<t.address<<"\n";auto r=t.run();pass&=r.cases>0&&!r.errors&&r.negative_errors==r.cases;
    out<<(first?"":",")<<"{\"address\":\"0x"<<t.address<<"\",\"cases\":"<<r.cases<<",\"errors\":"<<r.errors<<",\"negative_errors\":"<<r.negative_errors<<"}";first=false;}
  out<<"],\"passed\":"<<(pass?"true":"false")<<"}\n";return pass?0:1;
}
}
#define SNAP(...)
#define LEAF(A,P,B,O,R,I) REX_EXTERN(__imp__sub_##A); \
  const AddTest test_##A{{#A,I##Inputs.description,[]{return VerifySparseSynthetic(I##Inputs,O,R,B,__imp__sub_##A);}}};
