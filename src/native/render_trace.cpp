// T14: Graphics calls made by registered replacements, on the first root draw
// after each logical step. Xbox evidence: T08 render-boundary.md; root draw
// 824A7190, DoUpdateFrames 82478108. Existing port hooks call the helpers in
// render_trace.h explicitly; generated/original functions are untouched.
#include "replacement.h"
#include "render_trace.h"
#include "guest_memory.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>

#ifndef PVZ_RENDER_TRACE_TEST
REXCVAR_DEFINE_STRING(pvz_native_render_trace, "", "Diagnostics", "T14 JSONL Graphics trace inside replacements")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_UINT32(pvz_native_render_steps, 5000, "Diagnostics", "Complete logical-step draws to trace")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DECLARE(bool, pvz_replay_frame_locked);
namespace pvz { extern std::atomic<uint64_t> replay_step; }
#endif
namespace {
thread_local const char* scope = nullptr;
thread_local bool recording = false;
thread_local std::vector<std::string> calls;
std::atomic<uint64_t> drawn_step{0};
uint64_t frames = 0;
std::ofstream stream;
using pvz::guest::Read;
int64_t TraceClockSeconds(uint64_t step) { return int64_t(step / 100); }
std::string Bytes(const uint8_t* b, size_t n) {
  std::string result; result.reserve(n*2);
  constexpr char digits[]="0123456789abcdef";
  for(size_t i=0;i<n;++i) {result+=digits[b[i]>>4];result+=digits[b[i]&15];}
  return result;
}
std::string Text(uint8_t* base, uint32_t at) {
  const auto n=Read(base,at+0x14);
  if(n>8192) throw std::runtime_error("render trace invalid string length");
  const auto chars=Read(base,at+0x18)>=8?Read(base,at+4):at+4;
  return Bytes(base+chars,n*2);
}
std::string Narrow(uint8_t* base,uint32_t at) {
  const auto n=Read(base,at+20);
  if(n>8192) throw std::runtime_error("render trace invalid resource name");
  return Bytes(base+(Read(base,at+24)>=16?Read(base,at+4):at+4),n);
}
std::string FontKey(uint8_t* base,uint32_t font) {
  if(!font) return "null";
  if(Read(base,font)!=0x8207A134)
    return fmt::format("unidentified-font/{:08X}/{:08X}",Read(base,font),font);
  // Xbox 8255E550 copies FontData+20, point+24, tags+28, scale+88,
  // white+96. 82563410 -> 8255C4B8 assigns the source filename at
  // FontData+244 (8255C57C..590). Addresses themselves are allocator state.
  const auto data=Read(base,font+20);
  auto name=Narrow(base,data+244);
  if(name.empty()) return fmt::format("unidentified-font-data/{:08X}",font);
  auto key=fmt::format("{}/{}/{}/{}/{}",name,Bytes(base+font+4,16),
      Read(base,font+24),Bytes(base+font+88,8),unsigned(base[font+96]));
  const auto begin=Read(base,font+40),end=Read(base,font+44);
  if(end<begin || (end-begin)%28 || (end-begin)/28>256)
    throw std::runtime_error("render trace invalid font tags");
  for(auto tag=begin;tag<end;tag+=28) key+="/tag:"+Narrow(base,tag);
  return key;
}
std::string ImageKey(uint8_t* base,uint32_t image) {
  // 824AD008 constructs string+8 and width/height/rows/cols at +24..30.
  // Preserve an unknown pointer rather than equating anonymous images by size.
  if(!image) return "null";
  const auto path=Narrow(base,image+8);
  if(path.empty()) return fmt::format("unidentified-image/{:08X}",image);
  return fmt::format("{:08X}/{}/{}",Read(base,image),path,Bytes(base+image+36,16));
}
#ifndef PVZ_RENDER_TRACE_TEST
// Pointer arguments are dereferenced where Xbox proves the input extent.
// Stack addresses and scratch/LR registers are deliberately not draw arguments.
void Primitive(uint32_t address, const PPCContext& c, uint8_t* b) {
  if(!recording || !scope) return;
  const auto g=c.r3.u32;
  std::string args;
  switch(address) {
    case 0x8249B1A0: args=fmt::format("\"text\":\"{}\",\"x\":{},\"y\":{}",Text(b,c.r4.u32),c.r5.s32,c.r6.s32);break;
    case 0x8249A0C0: args=fmt::format("\"color\":\"{}\"",Bytes(b+c.r4.u32,16));break;
    case 0x8249A0B8: args=fmt::format("\"set_font\":\"{}\"",FontKey(b,c.r4.u32));break;
    case 0x8249BC50: args=fmt::format("\"image\":\"{}\",\"x\":{},\"y\":{},\"source\":\"{}\"",ImageKey(b,c.r4.u32),c.r5.s32,c.r6.s32,Bytes(b+c.r7.u32,16));break;
    case 0x8249C430: args=fmt::format("\"image\":\"{}\",\"matrix\":\"{}\"",ImageKey(b,c.r4.u32),Bytes(b+c.r5.u32,36));break;
    case 0x8249D0E8: args=fmt::format("\"image\":\"{}\",\"x\":{},\"y\":{}",ImageKey(b,c.r4.u32),c.r5.s32,c.r6.s32);break;
    case 0x8249D250: args=fmt::format("\"image\":\"{}\",\"x\":{},\"y\":{},\"cel\":{}",ImageKey(b,c.r4.u32),c.r5.s32,c.r6.s32,c.r7.s32);break;
    case 0x8249B4A0: args=fmt::format("\"x\":{},\"y\":{}",c.r4.s32,c.r5.s32);break;
    case 0x8249B4E8: args=fmt::format("\"f1\":{},\"f2\":{}",c.f1.u64,c.f2.u64);break;
    case 0x8249B508: args=fmt::format("\"f1\":{},\"f2\":{},\"f3\":{},\"f4\":{}",c.f1.u64,c.f2.u64,c.f3.u64,c.f4.u64);break;
    case 0x8249C4E0: args="\"clear\":true";break;
    default: args=fmt::format("\"r4\":{},\"r5\":{},\"r6\":{},\"r7\":{}",c.r4.u32,c.r5.u32,c.r6.u32,c.r7.u32);break;
  }
  // State is read on every operation, also covering inline writes to Graphics.
  calls.push_back(fmt::format("{{\"scope\":\"{}\",\"op\":\"{:08X}\",{},\"state\":\"{}\",\"font\":\"{}\",\"mode\":{},\"flags\":\"{}\"}}",
      scope,address,args,Bytes(b+g+8,0x38),FontKey(b,Read(b,g+0x40)),Read(b,g+0x44),Bytes(b+g+0x48,4)));
}
#endif
}
#ifndef PVZ_RENDER_TRACE_TEST
namespace pvz::native {
bool RenderTracing() {static const bool on=!REXCVAR_GET(pvz_native_render_trace).empty();return on;}
int64_t RenderClockSeconds() {
  // T12's logical tick is 10 ms. Preserve the real atlas alternation and its
  // one-second period, but make the diagnostic independent of process timing.
  if(RenderTracing()) return TraceClockSeconds(pvz::replay_step.load());
  return std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
RenderScope::RenderScope(const char* name):previous(scope) {scope=name;}
RenderScope::~RenderScope() {scope=previous;}
bool HoldRenderUpdate() {
  return RenderTracing() && REXCVAR_GET(pvz_replay_frame_locked) &&
      pvz::replay_step.load()>drawn_step.load();
}
void TraceGraphics(uint32_t address, const PPCContext& context, uint8_t* base) {
  if(RenderTracing()) Primitive(address,context,base);
}
}
REX_EXTERN(__imp__sub_824A7190);
REX_HOOK_RAW(sub_824A7190) {
  if(!pvz::native::RenderTracing()) {__imp__sub_824A7190(ctx,base);return;}
  if(!REXCVAR_GET(pvz_replay_frame_locked)) throw std::runtime_error("render trace requires frame-locked replay");
  const auto step=pvz::replay_step.load();
  const bool take=step>drawn_step.load() && frames<REXCVAR_GET(pvz_native_render_steps);
  recording=take;calls.clear();
  __imp__sub_824A7190(ctx,base);
  recording=false;drawn_step.store(step);
  if(!take) return;
  if(!stream.is_open()) {
    const auto path=std::filesystem::path(REXCVAR_GET(pvz_native_render_trace));
    if(path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    stream.open(path,std::ios::binary|std::ios::trunc);
    if(!stream) throw std::runtime_error("cannot open render trace");
    stream<<"{\"schema\":1,\"selection\":\"first-root-draw-after-each-logical-step\",\"image_sha256\":\"27e8e044a0b7e232ce43e1de2cf06b377040f3d24e67a86047b337bb46d257ea\"}\n";
  }
  stream<<"{\"step\":"<<step<<",\"calls\":[";
  for(size_t i=0;i<calls.size();++i) stream<<(i?",":"")<<calls[i];
  stream<<"]}\n";
  if(++frames==REXCVAR_GET(pvz_native_render_steps)) {
    stream<<"{\"complete\":true,\"frames\":"<<frames<<"}\n";
    REXLOG_INFO("PvZ native render completed steps {}",frames);
  }
  stream.flush();
}

#define TRACE(A,BODY) \
 REX_EXTERN(BODY); \
 REX_HOOK_RAW(sub_##A) { \
   pvz::native::TraceGraphics(0x##A,ctx,base); \
   BODY(ctx,base); \
 }
TRACE(8249B1A0,__imp__sub_8249B1A0)
TRACE(8249A0C0,__imp__sub_8249A0C0)
TRACE(8249A0B8,__imp__sub_8249A0B8)
TRACE(8249B588,__imp__sub_8249B588)
TRACE(8249B650,__imp__sub_8249B650)
TRACE(8249B4A0,__imp__sub_8249B4A0)
TRACE(8249B4E8,__imp__sub_8249B4E8)
TRACE(8249B508,__imp__sub_8249B508)
TRACE(8249C4E0,__imp__sub_8249C4E0)
TRACE(8249C590,__imp__sub_8249C590)
TRACE(8249C5D0,__imp__sub_8249C5D0)
#undef TRACE
#endif


