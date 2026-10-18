// T07b: policy approved 2026-10-17: one repeat tick per root logic update.
// The Xbox body below is a literal specialization of 82580D88, not Android/PC
// code. Only the existing repeat loop is wrapped; input branches remain exact.
// Source generated/default/pvz_recomp.92.cpp SHA256: 2929d0d6a5b9aa44edaafedc667809c7908eb5311e45dd56206c2cd4f4c63e3c
#include "pvz_funcs.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include "guest_memory.h"
#include <array>
#include <chrono>

// On by default since 2026-10-19: owner gamepad test and the body CTest passed.
REXCVAR_DEFINE_BOOL(pvz_gamepad_repeat_fix, true, "PvZ",
    "Advance gamepad repeat counters once per logical game update (100 Hz)");
REXCVAR_DEFINE_BOOL(pvz_gamepad_repeat_trace, false, "Diagnostics",
    "Trace raw pad edges, repeat deliveries, focus and accumulated logic ticks");

namespace {
using pvz::guest::Read;
struct Clock { uint32_t pad=0, manager=0, step=0; };
std::array<Clock,4> clocks;
uint32_t Manager(uint8_t* base) {
    const auto app=Read(base,0x82D0BC88);
    return app ? Read(base,app+0x36C) : 0;
}
uint32_t Step(uint8_t* base) {
    const auto manager=Manager(base);
    return manager ? Read(base,manager+0x38) : 0;
}
int64_t Milliseconds() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
uint32_t RepeatSteps(uint32_t pad,uint8_t* base) {
    const auto user=Read(base,pad+4);
    if (user>=clocks.size()) return 0;
    const auto manager=Manager(base);
    const auto step=Step(base);
    auto& clock=clocks[user];
    // A new/reconnected pad starts at the CURRENT step: disconnected time is
    // never charged to a newly pressed button. Counter rewind resets epoch.
    const bool reset=clock.pad!=pad || clock.manager!=manager || !base[pad+12];
    const auto elapsed=reset ? 0u : step-clock.step;
    static const bool trace=REXCVAR_GET(pvz_gamepad_repeat_trace);
    static unsigned observed=0;
    if (trace && (observed++<64 || elapsed>32))
        REXLOG_INFO("PvZ repeat clock user {} manager {:08X} step {} previous {} elapsed {} reset {}",
            user,manager,step,clock.step,elapsed,reset);
    clock={pad,manager,step};
    return elapsed<=0x7FFFFFFFu ? elapsed : 0u;
}
void TraceRepeat(const PPCContext& ctx,uint8_t* base) {
    static const bool trace=REXCVAR_GET(pvz_gamepad_repeat_trace);
    if (!trace) return;
    const auto pad=ctx.r31.u32;
    REXLOG_INFO("PvZ repeat event ms {} step {} user {} button {} flags {} source {} repeat_count {}",
        Milliseconds(),Step(base),ctx.r5.u32,ctx.r4.u32,ctx.r6.u32,ctx.r30.u32,
        Read(base,pad+0x8C+4*ctx.r30.u32));
}
void TraceResult(const PPCContext& ctx,uint8_t* base) {
    static const bool trace=REXCVAR_GET(pvz_gamepad_repeat_trace);
    if (!trace) return;
    const auto manager=Manager(base);
    const auto focus=manager ? Read(base,manager+0xA8) : 0;
    REXLOG_INFO("PvZ repeat result ms {} step {} user {} source {} focus {:08X} id {}",
        Milliseconds(),Step(base),Read(base,ctx.r31.u32+4),ctx.r30.u32,
        focus,focus ? Read(base,focus+0x6C) : 0);
}
struct Snapshot {
    uint32_t pad=0; bool native=false;
    std::array<uint8_t,20> held{};
    std::array<uint32_t,20> counts{},repeats{};
};
Snapshot Before(uint32_t pad,uint8_t* base,bool native) {
    static const bool trace=REXCVAR_GET(pvz_gamepad_repeat_trace);
    Snapshot before;
    if (trace) {
        before.pad=pad; before.native=native;
        for(unsigned i=0;i<20;++i) {
            before.held[i]=base[pad+0x28+i];
            before.counts[i]=Read(base,pad+0x3C+4*i);
            before.repeats[i]=Read(base,pad+0x8C+4*i);
        }
    }
    return before;
}
void After(const Snapshot& before,uint8_t* base) {
    if (!before.pad) return;
    const auto pad=before.pad;
    static unsigned completed=0;
    if(completed++<64) REXLOG_INFO("PvZ repeat poll completed pad {:08X} step {}",pad,Step(base));
    const auto manager=Manager(base);
    const auto focus=manager ? Read(base,manager+0xA8) : 0;
    bool changed=false;
    for(unsigned i=0;i<20;++i) {
        if(before.held[i]!=base[pad+0x28+i]) {
            changed=true;
            REXLOG_INFO("PvZ repeat edge ms {} step {} user {} button {} down {} focus {:08X} id {}",
                Milliseconds(),Step(base),Read(base,pad+4),i,unsigned(base[pad+0x28+i]),
                focus,focus ? Read(base,focus+0x6C) : 0);
        } else if(!before.native && before.held[i] &&
            (Read(base,pad+0x8C+4*i)>before.repeats[i] || Read(base,pad+0x3C+4*i)<before.counts[i])) {
            changed=true;
            REXLOG_INFO("PvZ repeat original ms {} step {} user {} source {} focus {:08X} id {}",
                Milliseconds(),Step(base),Read(base,pad+4),i,focus,focus ? Read(base,focus+0x6C) : 0);
        }
    }
    if (changed) {
        const auto app=Read(base,0x82D0BC88);
        const auto board=app ? Read(base,app+0x900) : 0;
        const auto user=Read(base,pad+4);
        if (board && Read(base,board)==0x820B728C && user<2) {
            const auto controls=Read(base,board+0x1B0+4*user);
            if (controls) REXLOG_INFO("PvZ repeat field ms {} step {} user {} mode {} scene {} paused {} x {} y {} packet {} state {}",
                Milliseconds(),Step(base),user,Read(base,app+0x9CC),Read(base,app+0x9D0),unsigned(base[board+0x1D9]),
                pvz::guest::Float(base,controls+0x58),pvz::guest::Float(base,controls+0x5C),
                Read(base,controls+0xBC),Read(base,controls+0x68));
        }
    }
}
}

REX_HOOK_RAW(sub_82580D88) {
    static const bool enabled = REXCVAR_GET(pvz_gamepad_repeat_fix);
    static const bool trace = REXCVAR_GET(pvz_gamepad_repeat_trace);
    if (!enabled) {
        if (!trace) { __imp__sub_82580D88(ctx, base); return; }
        const auto before = Before(ctx.r3.u32, base, false);
        __imp__sub_82580D88(ctx, base);
        After(before, base);
        return;
    }
    const uint32_t repeat_steps = RepeatSteps(ctx.r3.u32, base);
    const auto poll_before = Before(ctx.r3.u32, base, true);

	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x825fc830
	ctx.lr = 0x82580D90;
	__savegprlr_26(ctx, base);
	// stfd f29,-80(r1)
	ctx.fpscr.disableFlushMode();
	REX_STORE_U64(ctx.r1.u32 + -80, ctx.f29.u64);
	// stfd f30,-72(r1)
	REX_STORE_U64(ctx.r1.u32 + -72, ctx.f30.u64);
	// stfd f31,-64(r1)
	REX_STORE_U64(ctx.r1.u32 + -64, ctx.f31.u64);
	// stwu r1,-464(r1)
	ea = -464 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// lwz r3,4(r3)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r3.u32 + 4);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r3.s32, -1, ctx.xer);
	// beq cr6,0x82581880
	if (ctx.cr6.eq) goto loc_82581880;
	// addi r4,r31,380
	ctx.r4.s64 = ctx.r31.s64 + 380;
	// bl 0x822cb588
	ctx.lr = 0x82580DB8;
	sub_822CB588(ctx, base);
	// lis r11,-32256
	ctx.r11.s64 = -2113929216;
	// li r28,0
	ctx.r28.s64 = 0;
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
	// lfs f31,2708(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(ctx.r11.u32 + 2708);
	ctx.f31.f64 = double(temp.f32);
	// bne 0x82581788
	if (!ctx.cr0.eq) goto loc_82581788;
	// li r26,1
	ctx.r26.s64 = 1;
	// mr r30,r28
	ctx.r30.u64 = ctx.r28.u64;
	// stb r26,12(r31)
	REX_STORE_U8(ctx.r31.u32 + 12, ctx.r26.u8);
	// addi r27,r31,40
	ctx.r27.s64 = ctx.r31.s64 + 40;
// Only the counter path is clocked by completed root logic updates.
// Keep the scan/bookkeeping on zero-tick polls. Input polling above and
// ALL edge/axis/disconnection code below remain unchanged.
for (uint32_t tick = 0; tick < (repeat_steps ? repeat_steps : 1u); ++tick) {
    ctx.r30.u64 = 0;
loc_82580DDC:
	// lbzx r11,r27,r30
	ctx.r11.u64 = REX_LOAD_U8(ctx.r27.u32 + ctx.r30.u32);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq 0x82580ee4
	if (ctx.cr0.eq) goto loc_82580EE4;
    if (repeat_steps == 0) goto loc_82580EE4;
	// addi r11,r30,15
	ctx.r11.s64 = ctx.r30.s64 + 15;
	// addi r10,r30,55
	ctx.r10.s64 = ctx.r30.s64 + 55;
	// rlwinm r9,r11,2,0,29
	ctx.r9.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;
	// rlwinm r10,r10,2,0,29
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r9,r31
	ctx.r11.u64 = REX_LOAD_U32(ctx.r9.u32 + ctx.r31.u32);
	// lwzx r10,r10,r31
	ctx.r10.u64 = REX_LOAD_U32(ctx.r10.u32 + ctx.r31.u32);
	// addi r11,r11,1
	ctx.r11.s64 = ctx.r11.s64 + 1;
	// stwx r11,r9,r31
	REX_STORE_U32(ctx.r9.u32 + ctx.r31.u32, ctx.r11.u32);
	// cmpw cr6,r11,r10
	ctx.cr6.compare<int32_t>(ctx.r11.s32, ctx.r10.s32, ctx.xer);
	// ble cr6,0x82580ee4
	if (!ctx.cr6.gt) goto loc_82580EE4;
	// addi r11,r30,35
	ctx.r11.s64 = ctx.r30.s64 + 35;
	// rlwinm r10,r11,2,0,29
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r10,r31
	ctx.r11.u64 = REX_LOAD_U32(ctx.r10.u32 + ctx.r31.u32);
	// addi r11,r11,1
	ctx.r11.s64 = ctx.r11.s64 + 1;
	// stwx r11,r10,r31
	REX_STORE_U32(ctx.r10.u32 + ctx.r31.u32, ctx.r11.u32);
	// cmpwi cr6,r11,6
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 6, ctx.xer);
	// blt cr6,0x82580e30
	if (ctx.cr6.lt) goto loc_82580E30;
	// li r11,6
	ctx.r11.s64 = 6;
loc_82580E30:
	// stwx r11,r10,r31
	REX_STORE_U32(ctx.r10.u32 + ctx.r31.u32, ctx.r11.u32);
	// stwx r11,r9,r31
	REX_STORE_U32(ctx.r9.u32 + ctx.r31.u32, ctx.r11.u32);
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82580e60
	if (ctx.cr6.eq) goto loc_82580E60;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// li r6,1
	ctx.r6.s64 = 1;
	// mr r4,r30
	ctx.r4.u64 = ctx.r30.u64;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x82580E60;
	TraceRepeat(ctx, base);
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
    TraceResult(ctx, base);
loc_82580E60:
	// cmpwi cr6,r30,16
	ctx.cr6.compare<int32_t>(ctx.r30.s32, 16, ctx.xer);
	// beq cr6,0x82580ebc
	if (ctx.cr6.eq) goto loc_82580EBC;
	// cmpwi cr6,r30,17
	ctx.cr6.compare<int32_t>(ctx.r30.s32, 17, ctx.xer);
	// beq cr6,0x82580ea8
	if (ctx.cr6.eq) goto loc_82580EA8;
	// cmpwi cr6,r30,18
	ctx.cr6.compare<int32_t>(ctx.r30.s32, 18, ctx.xer);
	// beq cr6,0x82580e94
	if (ctx.cr6.eq) goto loc_82580E94;
	// cmpwi cr6,r30,19
	ctx.cr6.compare<int32_t>(ctx.r30.s32, 19, ctx.xer);
	// bne cr6,0x82580ee4
	if (!ctx.cr6.eq) goto loc_82580EE4;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82580ee4
	if (ctx.cr6.eq) goto loc_82580EE4;
	// li r4,3
	ctx.r4.s64 = 3;
	// b 0x82580ecc
	goto loc_82580ECC;
loc_82580E94:
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82580ee4
	if (ctx.cr6.eq) goto loc_82580EE4;
	// li r4,2
	ctx.r4.s64 = 2;
	// b 0x82580ecc
	goto loc_82580ECC;
loc_82580EA8:
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82580ee4
	if (ctx.cr6.eq) goto loc_82580EE4;
	// li r4,1
	ctx.r4.s64 = 1;
	// b 0x82580ecc
	goto loc_82580ECC;
loc_82580EBC:
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82580ee4
	if (ctx.cr6.eq) goto loc_82580EE4;
	// li r4,0
	ctx.r4.s64 = 0;
loc_82580ECC:
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// li r6,3
	ctx.r6.s64 = 3;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x82580EE4;
	TraceRepeat(ctx, base);
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
    TraceResult(ctx, base);
loc_82580EE4:
	// addi r30,r30,1
	ctx.r30.s64 = ctx.r30.s64 + 1;
	// cmpwi cr6,r30,20
	ctx.cr6.compare<int32_t>(ctx.r30.s32, 20, ctx.xer);
	// blt cr6,0x82580ddc
	if (ctx.cr6.lt) goto loc_82580DDC;
}

	// li r30,4
	ctx.r30.s64 = 4;
	// addi r29,r31,316
	ctx.r29.s64 = ctx.r31.s64 + 316;
loc_82580EF8:
	// cmpwi cr6,r30,12
	ctx.cr6.compare<int32_t>(ctx.r30.s32, 12, ctx.xer);
	// beq cr6,0x82580f8c
	if (ctx.cr6.eq) goto loc_82580F8C;
	// cmpwi cr6,r30,13
	ctx.cr6.compare<int32_t>(ctx.r30.s32, 13, ctx.xer);
	// beq cr6,0x82580f8c
	if (ctx.cr6.eq) goto loc_82580F8C;
	// lbzx r11,r27,r30
	ctx.r11.u64 = REX_LOAD_U8(ctx.r27.u32 + ctx.r30.u32);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// bne 0x82580f48
	if (!ctx.cr0.eq) goto loc_82580F48;
	// lhz r10,384(r31)
	ctx.r10.u64 = REX_LOAD_U16(ctx.r31.u32 + 384);
	// lwz r9,0(r29)
	ctx.r9.u64 = REX_LOAD_U32(ctx.r29.u32 + 0);
	// and. r10,r10,r9
	ctx.r10.u64 = ctx.r10.u64 & ctx.r9.u64;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beq 0x82580f48
	if (ctx.cr0.eq) goto loc_82580F48;
	// stbx r26,r27,r30
	REX_STORE_U8(ctx.r27.u32 + ctx.r30.u32, ctx.r26.u8);
	// stw r28,-240(r29)
	REX_STORE_U32(ctx.r29.u32 + -240, ctx.r28.u32);
	// stw r28,-160(r29)
	REX_STORE_U32(ctx.r29.u32 + -160, ctx.r28.u32);
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82580f8c
	if (ctx.cr6.eq) goto loc_82580F8C;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// b 0x82580f78
	goto loc_82580F78;
loc_82580F48:
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// bne cr6,0x82580f8c
	if (!ctx.cr6.eq) goto loc_82580F8C;
	// lhz r11,384(r31)
	ctx.r11.u64 = REX_LOAD_U16(ctx.r31.u32 + 384);
	// lwz r10,0(r29)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r29.u32 + 0);
	// and. r11,r11,r10
	ctx.r11.u64 = ctx.r11.u64 & ctx.r10.u64;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne 0x82580f8c
	if (!ctx.cr0.eq) goto loc_82580F8C;
	// stbx r28,r27,r30
	REX_STORE_U8(ctx.r27.u32 + ctx.r30.u32, ctx.r28.u8);
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82580f8c
	if (ctx.cr6.eq) goto loc_82580F8C;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
loc_82580F78:
	// li r6,0
	ctx.r6.s64 = 0;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// mr r4,r30
	ctx.r4.u64 = ctx.r30.u64;
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x82580F8C;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_82580F8C:
	// addi r30,r30,1
	ctx.r30.s64 = ctx.r30.s64 + 1;
	// addi r29,r29,4
	ctx.r29.s64 = ctx.r29.s64 + 4;
	// cmpwi cr6,r30,15
	ctx.cr6.compare<int32_t>(ctx.r30.s32, 15, ctx.xer);
	// ble cr6,0x82580ef8
	if (!ctx.cr6.gt) goto loc_82580EF8;
	// lbz r11,52(r31)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r31.u32 + 52);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// bne 0x82580fd8
	if (!ctx.cr0.eq) goto loc_82580FD8;
	// lbz r10,386(r31)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r31.u32 + 386);
	// cmplwi cr6,r10,30
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, 30, ctx.xer);
	// ble cr6,0x82580fd8
	if (!ctx.cr6.gt) goto loc_82580FD8;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r26,52(r31)
	REX_STORE_U8(ctx.r31.u32 + 52, ctx.r26.u8);
	// stw r28,188(r31)
	REX_STORE_U32(ctx.r31.u32 + 188, ctx.r28.u32);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// stw r28,108(r31)
	REX_STORE_U32(ctx.r31.u32 + 108, ctx.r28.u32);
	// beq cr6,0x82581018
	if (ctx.cr6.eq) goto loc_82581018;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// b 0x82581004
	goto loc_82581004;
loc_82580FD8:
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// bne cr6,0x82581018
	if (!ctx.cr6.eq) goto loc_82581018;
	// lbz r11,386(r31)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r31.u32 + 386);
	// cmplwi cr6,r11,30
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 30, ctx.xer);
	// bgt cr6,0x82581018
	if (ctx.cr6.gt) goto loc_82581018;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r28,52(r31)
	REX_STORE_U8(ctx.r31.u32 + 52, ctx.r28.u8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82581018
	if (ctx.cr6.eq) goto loc_82581018;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
loc_82581004:
	// li r6,0
	ctx.r6.s64 = 0;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// li r4,12
	ctx.r4.s64 = 12;
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x82581018;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_82581018:
	// lbz r11,53(r31)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r31.u32 + 53);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// bne 0x82581054
	if (!ctx.cr0.eq) goto loc_82581054;
	// lbz r10,387(r31)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r31.u32 + 387);
	// cmplwi cr6,r10,30
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, 30, ctx.xer);
	// ble cr6,0x82581054
	if (!ctx.cr6.gt) goto loc_82581054;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r26,53(r31)
	REX_STORE_U8(ctx.r31.u32 + 53, ctx.r26.u8);
	// stw r28,192(r31)
	REX_STORE_U32(ctx.r31.u32 + 192, ctx.r28.u32);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// stw r28,112(r31)
	REX_STORE_U32(ctx.r31.u32 + 112, ctx.r28.u32);
	// beq cr6,0x82581094
	if (ctx.cr6.eq) goto loc_82581094;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// b 0x82581080
	goto loc_82581080;
loc_82581054:
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// bne cr6,0x82581094
	if (!ctx.cr6.eq) goto loc_82581094;
	// lbz r11,387(r31)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r31.u32 + 387);
	// cmplwi cr6,r11,30
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 30, ctx.xer);
	// bgt cr6,0x82581094
	if (ctx.cr6.gt) goto loc_82581094;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r28,53(r31)
	REX_STORE_U8(ctx.r31.u32 + 53, ctx.r28.u8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82581094
	if (ctx.cr6.eq) goto loc_82581094;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
loc_82581080:
	// li r6,0
	ctx.r6.s64 = 0;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// li r4,13
	ctx.r4.s64 = 13;
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x82581094;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_82581094:
	// lha r11,388(r31)
	ctx.r11.s64 = int16_t(REX_LOAD_U16(ctx.r31.u32 + 388));
	// lis r10,-32248
	ctx.r10.s64 = -2113404928;
	// mr r8,r11
	ctx.r8.u64 = ctx.r11.u64;
	// std r8,80(r1)
	REX_STORE_U64(ctx.r1.u32 + 80, ctx.r8.u64);
	// lha r9,390(r31)
	ctx.r9.s64 = int16_t(REX_LOAD_U16(ctx.r31.u32 + 390));
	// lfs f30,-9536(r10)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(ctx.r10.u32 + -9536);
	ctx.f30.f64 = double(temp.f32);
	// stw r11,16(r31)
	REX_STORE_U32(ctx.r31.u32 + 16, ctx.r11.u32);
	// mr r11,r9
	ctx.r11.u64 = ctx.r9.u64;
	// stw r9,20(r31)
	REX_STORE_U32(ctx.r31.u32 + 20, ctx.r9.u32);
	// lfd f0,80(r1)
	ctx.f0.u64 = REX_LOAD_U64(ctx.r1.u32 + 80);
	// std r9,80(r1)
	REX_STORE_U64(ctx.r1.u32 + 80, ctx.r9.u64);
	// lfd f13,80(r1)
	ctx.f13.u64 = REX_LOAD_U64(ctx.r1.u32 + 80);
	// fcfid f0,f0
	ctx.f0.f64 = double(ctx.f0.s64);
	// fcfid f13,f13
	ctx.f13.f64 = double(ctx.f13.s64);
	// frsp f13,f13
	ctx.f13.f64 = double(float(ctx.f13.f64));
	// frsp f0,f0
	ctx.f0.f64 = double(float(ctx.f0.f64));
	// fmuls f12,f13,f13
	ctx.f12.f64 = double(float(ctx.f13.f64 * ctx.f13.f64));
	// fmuls f11,f0,f0
	ctx.f11.f64 = double(float(ctx.f0.f64 * ctx.f0.f64));
	// fadds f12,f12,f11
	ctx.f12.f64 = double(float(ctx.f12.f64 + ctx.f11.f64));
	// fsqrts f12,f12
	ctx.f12.f64 = double(float(sqrt(ctx.f12.f64)));
	// fsubs f12,f12,f30
	ctx.f12.f64 = double(float(ctx.f12.f64 - ctx.f30.f64));
	// fmuls f0,f0,f12
	ctx.f0.f64 = double(float(ctx.f0.f64 * ctx.f12.f64));
	// fmuls f29,f13,f12
	ctx.f29.f64 = double(float(ctx.f13.f64 * ctx.f12.f64));
	// fcmpu cr6,f12,f31
	ctx.cr6.compare(ctx.f12.f64, ctx.f31.f64);
	// ble cr6,0x8258115c
	if (!ctx.cr6.gt) goto loc_8258115C;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82581128
	if (ctx.cr6.eq) goto loc_82581128;
	// fctiwz f0,f0
	ctx.f0.s64 = std::isnan(ctx.f0.f64) ? int64_t(0x80000000U) : (ctx.f0.f64 >= double(INT_MAX)) ? INT_MAX : simde_mm_cvttsd_si32(simde_mm_load_sd(&ctx.f0.f64));
	// stfd f0,80(r1)
	REX_STORE_U64(ctx.r1.u32 + 80, ctx.f0.u64);
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// li r4,0
	ctx.r4.s64 = 0;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r11,8(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 8);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// lwz r6,84(r1)
	ctx.r6.u64 = REX_LOAD_U32(ctx.r1.u32 + 84);
	// bctrl 
	ctx.lr = 0x82581128;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_82581128:
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82581164
	if (ctx.cr6.eq) goto loc_82581164;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// fctiwz f0,f29
	ctx.fpscr.disableFlushMode();
	ctx.f0.s64 = std::isnan(ctx.f29.f64) ? int64_t(0x80000000U) : (ctx.f29.f64 >= double(INT_MAX)) ? INT_MAX : simde_mm_cvttsd_si32(simde_mm_load_sd(&ctx.f29.f64));
	// li r4,1
	ctx.r4.s64 = 1;
	// stfd f0,80(r1)
	REX_STORE_U64(ctx.r1.u32 + 80, ctx.f0.u64);
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r6,84(r1)
	ctx.r6.u64 = REX_LOAD_U32(ctx.r1.u32 + 84);
	// lwz r11,8(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 8);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x82581158;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
	// b 0x82581164
	goto loc_82581164;
loc_8258115C:
	// stw r28,16(r31)
	REX_STORE_U32(ctx.r31.u32 + 16, ctx.r28.u32);
	// stw r28,20(r31)
	REX_STORE_U32(ctx.r31.u32 + 20, ctx.r28.u32);
loc_82581164:
	// lha r11,394(r31)
	ctx.r11.s64 = int16_t(REX_LOAD_U16(ctx.r31.u32 + 394));
	// mr r9,r11
	ctx.r9.u64 = ctx.r11.u64;
	// lha r10,392(r31)
	ctx.r10.s64 = int16_t(REX_LOAD_U16(ctx.r31.u32 + 392));
	// std r9,88(r1)
	REX_STORE_U64(ctx.r1.u32 + 88, ctx.r9.u64);
	// lfd f13,88(r1)
	ctx.fpscr.disableFlushMode();
	ctx.f13.u64 = REX_LOAD_U64(ctx.r1.u32 + 88);
	// std r10,80(r1)
	REX_STORE_U64(ctx.r1.u32 + 80, ctx.r10.u64);
	// lfd f0,80(r1)
	ctx.f0.u64 = REX_LOAD_U64(ctx.r1.u32 + 80);
	// fcfid f0,f0
	ctx.f0.f64 = double(ctx.f0.s64);
	// stw r11,36(r31)
	REX_STORE_U32(ctx.r31.u32 + 36, ctx.r11.u32);
	// fcfid f13,f13
	ctx.f13.f64 = double(ctx.f13.s64);
	// mr r11,r10
	ctx.r11.u64 = ctx.r10.u64;
	// frsp f0,f0
	ctx.f0.f64 = double(float(ctx.f0.f64));
	// stfs f0,80(r1)
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r1.u32 + 80, temp.u32);
	// stw r10,32(r31)
	REX_STORE_U32(ctx.r31.u32 + 32, ctx.r10.u32);
	// frsp f13,f13
	ctx.f13.f64 = double(float(ctx.f13.f64));
	// stfs f13,84(r1)
	temp.f32 = float(ctx.f13.f64);
	REX_STORE_U32(ctx.r1.u32 + 84, temp.u32);
	// fmuls f12,f0,f0
	ctx.f12.f64 = double(float(ctx.f0.f64 * ctx.f0.f64));
	// ld r11,80(r1)
	ctx.r11.u64 = REX_LOAD_U64(ctx.r1.u32 + 80);
	// std r11,88(r1)
	REX_STORE_U64(ctx.r1.u32 + 88, ctx.r11.u64);
	// fmuls f13,f13,f13
	ctx.f13.f64 = double(float(ctx.f13.f64 * ctx.f13.f64));
	// lfs f11,92(r1)
	temp.u32 = REX_LOAD_U32(ctx.r1.u32 + 92);
	ctx.f11.f64 = double(temp.f32);
	// fadds f13,f13,f12
	ctx.f13.f64 = double(float(ctx.f13.f64 + ctx.f12.f64));
	// fsqrts f13,f13
	ctx.f13.f64 = double(float(sqrt(ctx.f13.f64)));
	// fsubs f13,f13,f30
	ctx.f13.f64 = double(float(ctx.f13.f64 - ctx.f30.f64));
	// fmuls f0,f0,f13
	ctx.f0.f64 = double(float(ctx.f0.f64 * ctx.f13.f64));
	// fmuls f30,f11,f13
	ctx.f30.f64 = double(float(ctx.f11.f64 * ctx.f13.f64));
	// fcmpu cr6,f13,f31
	ctx.cr6.compare(ctx.f13.f64, ctx.f31.f64);
	// ble cr6,0x82581238
	if (!ctx.cr6.gt) goto loc_82581238;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82581204
	if (ctx.cr6.eq) goto loc_82581204;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// fctiwz f0,f0
	ctx.f0.s64 = std::isnan(ctx.f0.f64) ? int64_t(0x80000000U) : (ctx.f0.f64 >= double(INT_MAX)) ? INT_MAX : simde_mm_cvttsd_si32(simde_mm_load_sd(&ctx.f0.f64));
	// li r4,4
	ctx.r4.s64 = 4;
	// stfd f0,88(r1)
	REX_STORE_U64(ctx.r1.u32 + 88, ctx.f0.u64);
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r6,92(r1)
	ctx.r6.u64 = REX_LOAD_U32(ctx.r1.u32 + 92);
	// lwz r11,8(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 8);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x82581204;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_82581204:
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82581240
	if (ctx.cr6.eq) goto loc_82581240;
	// fctiwz f0,f30
	ctx.fpscr.disableFlushMode();
	ctx.f0.s64 = std::isnan(ctx.f30.f64) ? int64_t(0x80000000U) : (ctx.f30.f64 >= double(INT_MAX)) ? INT_MAX : simde_mm_cvttsd_si32(simde_mm_load_sd(&ctx.f30.f64));
	// stfd f0,88(r1)
	REX_STORE_U64(ctx.r1.u32 + 88, ctx.f0.u64);
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// li r4,5
	ctx.r4.s64 = 5;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r11,8(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 8);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// lwz r6,92(r1)
	ctx.r6.u64 = REX_LOAD_U32(ctx.r1.u32 + 92);
	// bctrl 
	ctx.lr = 0x82581234;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
	// b 0x82581240
	goto loc_82581240;
loc_82581238:
	// stw r28,32(r31)
	REX_STORE_U32(ctx.r31.u32 + 32, ctx.r28.u32);
	// stw r28,36(r31)
	REX_STORE_U32(ctx.r31.u32 + 36, ctx.r28.u32);
loc_82581240:
	// lbz r11,56(r31)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r31.u32 + 56);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// bne 0x825812a4
	if (!ctx.cr0.eq) goto loc_825812A4;
	// lhz r10,384(r31)
	ctx.r10.u64 = REX_LOAD_U16(ctx.r31.u32 + 384);
	// clrlwi. r10,r10,31
	ctx.r10.u64 = ctx.r10.u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beq 0x825812a4
	if (ctx.cr0.eq) goto loc_825812A4;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r26,56(r31)
	REX_STORE_U8(ctx.r31.u32 + 56, ctx.r26.u8);
	// stw r28,124(r31)
	REX_STORE_U32(ctx.r31.u32 + 124, ctx.r28.u32);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// stw r28,204(r31)
	REX_STORE_U32(ctx.r31.u32 + 204, ctx.r28.u32);
	// beq cr6,0x8258128c
	if (ctx.cr6.eq) goto loc_8258128C;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// li r6,0
	ctx.r6.s64 = 0;
	// li r4,16
	ctx.r4.s64 = 16;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x8258128C;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_8258128C:
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x8258130c
	if (ctx.cr6.eq) goto loc_8258130C;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// b 0x825812f8
	goto loc_825812F8;
loc_825812A4:
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// bne cr6,0x8258130c
	if (!ctx.cr6.eq) goto loc_8258130C;
	// lhz r11,384(r31)
	ctx.r11.u64 = REX_LOAD_U16(ctx.r31.u32 + 384);
	// clrlwi. r11,r11,31
	ctx.r11.u64 = ctx.r11.u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne 0x8258130c
	if (!ctx.cr0.eq) goto loc_8258130C;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r28,56(r31)
	REX_STORE_U8(ctx.r31.u32 + 56, ctx.r28.u8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x825812e4
	if (ctx.cr6.eq) goto loc_825812E4;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// li r6,0
	ctx.r6.s64 = 0;
	// li r4,16
	ctx.r4.s64 = 16;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x825812E4;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_825812E4:
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x8258130c
	if (ctx.cr6.eq) goto loc_8258130C;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
loc_825812F8:
	// li r6,2
	ctx.r6.s64 = 2;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// li r4,0
	ctx.r4.s64 = 0;
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x8258130C;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_8258130C:
	// lbz r11,57(r31)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r31.u32 + 57);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// bne 0x82581370
	if (!ctx.cr0.eq) goto loc_82581370;
	// lhz r10,384(r31)
	ctx.r10.u64 = REX_LOAD_U16(ctx.r31.u32 + 384);
	// rlwinm. r10,r10,0,30,30
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beq 0x82581370
	if (ctx.cr0.eq) goto loc_82581370;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r26,57(r31)
	REX_STORE_U8(ctx.r31.u32 + 57, ctx.r26.u8);
	// stw r28,128(r31)
	REX_STORE_U32(ctx.r31.u32 + 128, ctx.r28.u32);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// stw r28,208(r31)
	REX_STORE_U32(ctx.r31.u32 + 208, ctx.r28.u32);
	// beq cr6,0x82581358
	if (ctx.cr6.eq) goto loc_82581358;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// li r6,0
	ctx.r6.s64 = 0;
	// li r4,17
	ctx.r4.s64 = 17;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x82581358;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_82581358:
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x825813d8
	if (ctx.cr6.eq) goto loc_825813D8;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// b 0x825813c4
	goto loc_825813C4;
loc_82581370:
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// bne cr6,0x825813d8
	if (!ctx.cr6.eq) goto loc_825813D8;
	// lhz r11,384(r31)
	ctx.r11.u64 = REX_LOAD_U16(ctx.r31.u32 + 384);
	// rlwinm. r11,r11,0,30,30
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne 0x825813d8
	if (!ctx.cr0.eq) goto loc_825813D8;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r28,57(r31)
	REX_STORE_U8(ctx.r31.u32 + 57, ctx.r28.u8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x825813b0
	if (ctx.cr6.eq) goto loc_825813B0;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// li r6,0
	ctx.r6.s64 = 0;
	// li r4,17
	ctx.r4.s64 = 17;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x825813B0;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_825813B0:
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x825813d8
	if (ctx.cr6.eq) goto loc_825813D8;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
loc_825813C4:
	// li r6,2
	ctx.r6.s64 = 2;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// li r4,1
	ctx.r4.s64 = 1;
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x825813D8;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_825813D8:
	// lbz r11,58(r31)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r31.u32 + 58);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// bne 0x8258143c
	if (!ctx.cr0.eq) goto loc_8258143C;
	// lhz r10,384(r31)
	ctx.r10.u64 = REX_LOAD_U16(ctx.r31.u32 + 384);
	// rlwinm. r10,r10,0,29,29
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beq 0x8258143c
	if (ctx.cr0.eq) goto loc_8258143C;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r26,58(r31)
	REX_STORE_U8(ctx.r31.u32 + 58, ctx.r26.u8);
	// stw r28,132(r31)
	REX_STORE_U32(ctx.r31.u32 + 132, ctx.r28.u32);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// stw r28,212(r31)
	REX_STORE_U32(ctx.r31.u32 + 212, ctx.r28.u32);
	// beq cr6,0x82581424
	if (ctx.cr6.eq) goto loc_82581424;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// li r6,0
	ctx.r6.s64 = 0;
	// li r4,18
	ctx.r4.s64 = 18;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x82581424;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_82581424:
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x825814a4
	if (ctx.cr6.eq) goto loc_825814A4;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// b 0x82581490
	goto loc_82581490;
loc_8258143C:
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// bne cr6,0x825814a4
	if (!ctx.cr6.eq) goto loc_825814A4;
	// lhz r11,384(r31)
	ctx.r11.u64 = REX_LOAD_U16(ctx.r31.u32 + 384);
	// rlwinm. r11,r11,0,29,29
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne 0x825814a4
	if (!ctx.cr0.eq) goto loc_825814A4;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r28,58(r31)
	REX_STORE_U8(ctx.r31.u32 + 58, ctx.r28.u8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x8258147c
	if (ctx.cr6.eq) goto loc_8258147C;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// li r6,0
	ctx.r6.s64 = 0;
	// li r4,18
	ctx.r4.s64 = 18;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x8258147C;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_8258147C:
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x825814a4
	if (ctx.cr6.eq) goto loc_825814A4;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
loc_82581490:
	// li r6,2
	ctx.r6.s64 = 2;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// li r4,2
	ctx.r4.s64 = 2;
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x825814A4;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_825814A4:
	// lbz r11,59(r31)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r31.u32 + 59);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// bne 0x82581508
	if (!ctx.cr0.eq) goto loc_82581508;
	// lhz r10,384(r31)
	ctx.r10.u64 = REX_LOAD_U16(ctx.r31.u32 + 384);
	// rlwinm. r10,r10,0,28,28
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 0) & 0x8;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beq 0x82581508
	if (ctx.cr0.eq) goto loc_82581508;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r26,59(r31)
	REX_STORE_U8(ctx.r31.u32 + 59, ctx.r26.u8);
	// stw r28,136(r31)
	REX_STORE_U32(ctx.r31.u32 + 136, ctx.r28.u32);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// stw r28,216(r31)
	REX_STORE_U32(ctx.r31.u32 + 216, ctx.r28.u32);
	// beq cr6,0x825814f0
	if (ctx.cr6.eq) goto loc_825814F0;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// li r6,0
	ctx.r6.s64 = 0;
	// li r4,19
	ctx.r4.s64 = 19;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x825814F0;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_825814F0:
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82581570
	if (ctx.cr6.eq) goto loc_82581570;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// b 0x8258155c
	goto loc_8258155C;
loc_82581508:
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// bne cr6,0x82581570
	if (!ctx.cr6.eq) goto loc_82581570;
	// lhz r11,384(r31)
	ctx.r11.u64 = REX_LOAD_U16(ctx.r31.u32 + 384);
	// rlwinm. r11,r11,0,28,28
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0x8;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne 0x82581570
	if (!ctx.cr0.eq) goto loc_82581570;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r28,59(r31)
	REX_STORE_U8(ctx.r31.u32 + 59, ctx.r28.u8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82581548
	if (ctx.cr6.eq) goto loc_82581548;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// li r6,0
	ctx.r6.s64 = 0;
	// li r4,19
	ctx.r4.s64 = 19;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x82581548;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_82581548:
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82581570
	if (ctx.cr6.eq) goto loc_82581570;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
loc_8258155C:
	// li r6,2
	ctx.r6.s64 = 2;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// li r4,3
	ctx.r4.s64 = 3;
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x82581570;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_82581570:
	// lis r30,-32131
	ctx.r30.s64 = -2105737216;
	// lbz r10,0(r27)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r27.u32 + 0);
	// cmplwi r10,0
	ctx.cr0.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	// lwz r11,22592(r30)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r30.u32 + 22592);
	// bne 0x825815b4
	if (!ctx.cr0.eq) goto loc_825815B4;
	// lwz r9,20(r31)
	ctx.r9.u64 = REX_LOAD_U32(ctx.r31.u32 + 20);
	// cmpw cr6,r9,r11
	ctx.cr6.compare<int32_t>(ctx.r9.s32, ctx.r11.s32, ctx.xer);
	// ble cr6,0x825815b4
	if (!ctx.cr6.gt) goto loc_825815B4;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r26,0(r27)
	REX_STORE_U8(ctx.r27.u32 + 0, ctx.r26.u8);
	// stw r28,60(r31)
	REX_STORE_U32(ctx.r31.u32 + 60, ctx.r28.u32);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// stw r28,140(r31)
	REX_STORE_U32(ctx.r31.u32 + 140, ctx.r28.u32);
	// beq cr6,0x825815f8
	if (ctx.cr6.eq) goto loc_825815F8;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// b 0x825815e0
	goto loc_825815E0;
loc_825815B4:
	// cmplwi cr6,r10,1
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, 1, ctx.xer);
	// bne cr6,0x825815f8
	if (!ctx.cr6.eq) goto loc_825815F8;
	// lwz r10,20(r31)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + 20);
	// cmpw cr6,r10,r11
	ctx.cr6.compare<int32_t>(ctx.r10.s32, ctx.r11.s32, ctx.xer);
	// bgt cr6,0x825815f8
	if (ctx.cr6.gt) goto loc_825815F8;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r28,0(r27)
	REX_STORE_U8(ctx.r27.u32 + 0, ctx.r28.u8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x825815f8
	if (ctx.cr6.eq) goto loc_825815F8;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
loc_825815E0:
	// li r6,0
	ctx.r6.s64 = 0;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// li r4,0
	ctx.r4.s64 = 0;
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x825815F4;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
	// lwz r11,22592(r30)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r30.u32 + 22592);
loc_825815F8:
	// lbz r10,41(r31)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r31.u32 + 41);
	// cmplwi r10,0
	ctx.cr0.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	// bne 0x82581638
	if (!ctx.cr0.eq) goto loc_82581638;
	// lwz r9,20(r31)
	ctx.r9.u64 = REX_LOAD_U32(ctx.r31.u32 + 20);
	// neg r8,r11
	ctx.r8.s64 = static_cast<int64_t>(-ctx.r11.u64);
	// cmpw cr6,r9,r8
	ctx.cr6.compare<int32_t>(ctx.r9.s32, ctx.r8.s32, ctx.xer);
	// bge cr6,0x82581638
	if (!ctx.cr6.lt) goto loc_82581638;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r26,41(r31)
	REX_STORE_U8(ctx.r31.u32 + 41, ctx.r26.u8);
	// stw r28,64(r31)
	REX_STORE_U32(ctx.r31.u32 + 64, ctx.r28.u32);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// stw r28,144(r31)
	REX_STORE_U32(ctx.r31.u32 + 144, ctx.r28.u32);
	// beq cr6,0x82581680
	if (ctx.cr6.eq) goto loc_82581680;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// b 0x82581668
	goto loc_82581668;
loc_82581638:
	// cmplwi cr6,r10,1
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, 1, ctx.xer);
	// bne cr6,0x82581680
	if (!ctx.cr6.eq) goto loc_82581680;
	// lwz r10,20(r31)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + 20);
	// neg r9,r11
	ctx.r9.s64 = static_cast<int64_t>(-ctx.r11.u64);
	// cmpw cr6,r10,r9
	ctx.cr6.compare<int32_t>(ctx.r10.s32, ctx.r9.s32, ctx.xer);
	// blt cr6,0x82581680
	if (ctx.cr6.lt) goto loc_82581680;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r28,41(r31)
	REX_STORE_U8(ctx.r31.u32 + 41, ctx.r28.u8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82581680
	if (ctx.cr6.eq) goto loc_82581680;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
loc_82581668:
	// li r6,0
	ctx.r6.s64 = 0;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// li r4,1
	ctx.r4.s64 = 1;
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x8258167C;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
	// lwz r11,22592(r30)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r30.u32 + 22592);
loc_82581680:
	// lbz r10,42(r31)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r31.u32 + 42);
	// cmplwi r10,0
	ctx.cr0.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	// bne 0x825816c0
	if (!ctx.cr0.eq) goto loc_825816C0;
	// lwz r9,16(r31)
	ctx.r9.u64 = REX_LOAD_U32(ctx.r31.u32 + 16);
	// neg r8,r11
	ctx.r8.s64 = static_cast<int64_t>(-ctx.r11.u64);
	// cmpw cr6,r9,r8
	ctx.cr6.compare<int32_t>(ctx.r9.s32, ctx.r8.s32, ctx.xer);
	// bge cr6,0x825816c0
	if (!ctx.cr6.lt) goto loc_825816C0;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r26,42(r31)
	REX_STORE_U8(ctx.r31.u32 + 42, ctx.r26.u8);
	// stw r28,68(r31)
	REX_STORE_U32(ctx.r31.u32 + 68, ctx.r28.u32);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// stw r28,148(r31)
	REX_STORE_U32(ctx.r31.u32 + 148, ctx.r28.u32);
	// beq cr6,0x82581708
	if (ctx.cr6.eq) goto loc_82581708;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// b 0x825816f0
	goto loc_825816F0;
loc_825816C0:
	// cmplwi cr6,r10,1
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, 1, ctx.xer);
	// bne cr6,0x82581708
	if (!ctx.cr6.eq) goto loc_82581708;
	// lwz r10,16(r31)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + 16);
	// neg r9,r11
	ctx.r9.s64 = static_cast<int64_t>(-ctx.r11.u64);
	// cmpw cr6,r10,r9
	ctx.cr6.compare<int32_t>(ctx.r10.s32, ctx.r9.s32, ctx.xer);
	// blt cr6,0x82581708
	if (ctx.cr6.lt) goto loc_82581708;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r28,42(r31)
	REX_STORE_U8(ctx.r31.u32 + 42, ctx.r28.u8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x82581708
	if (ctx.cr6.eq) goto loc_82581708;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
loc_825816F0:
	// li r6,0
	ctx.r6.s64 = 0;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// li r4,2
	ctx.r4.s64 = 2;
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x82581704;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
	// lwz r11,22592(r30)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r30.u32 + 22592);
loc_82581708:
	// lbz r10,43(r31)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r31.u32 + 43);
	// cmplwi r10,0
	ctx.cr0.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	// bne 0x82581744
	if (!ctx.cr0.eq) goto loc_82581744;
	// lwz r9,16(r31)
	ctx.r9.u64 = REX_LOAD_U32(ctx.r31.u32 + 16);
	// cmpw cr6,r9,r11
	ctx.cr6.compare<int32_t>(ctx.r9.s32, ctx.r11.s32, ctx.xer);
	// ble cr6,0x82581744
	if (!ctx.cr6.gt) goto loc_82581744;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r26,43(r31)
	REX_STORE_U8(ctx.r31.u32 + 43, ctx.r26.u8);
	// stw r28,72(r31)
	REX_STORE_U32(ctx.r31.u32 + 72, ctx.r28.u32);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// stw r28,152(r31)
	REX_STORE_U32(ctx.r31.u32 + 152, ctx.r28.u32);
	// beq cr6,0x825817f4
	if (ctx.cr6.eq) goto loc_825817F4;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// b 0x82581770
	goto loc_82581770;
loc_82581744:
	// cmplwi cr6,r10,1
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, 1, ctx.xer);
	// bne cr6,0x825817f4
	if (!ctx.cr6.eq) goto loc_825817F4;
	// lwz r10,16(r31)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + 16);
	// cmpw cr6,r10,r11
	ctx.cr6.compare<int32_t>(ctx.r10.s32, ctx.r11.s32, ctx.xer);
	// bgt cr6,0x825817f4
	if (ctx.cr6.gt) goto loc_825817F4;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// stb r28,43(r31)
	REX_STORE_U8(ctx.r31.u32 + 43, ctx.r28.u8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x825817f4
	if (ctx.cr6.eq) goto loc_825817F4;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
loc_82581770:
	// li r4,3
	ctx.r4.s64 = 3;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// li r6,0
	ctx.r6.s64 = 0;
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x82581784;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
	// b 0x825817f4
	goto loc_825817F4;
loc_82581788:
	// cmpwi cr6,r3,1167
	ctx.cr6.compare<int32_t>(ctx.r3.s32, 1167, ctx.xer);
	// bne cr6,0x825817f4
	if (!ctx.cr6.eq) goto loc_825817F4;
	// mr r30,r28
	ctx.r30.u64 = ctx.r28.u64;
	// addi r29,r31,40
	ctx.r29.s64 = ctx.r31.s64 + 40;
loc_82581798:
	// lbzx r11,r29,r30
	ctx.r11.u64 = REX_LOAD_U8(ctx.r29.u32 + ctx.r30.u32);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq 0x825817cc
	if (ctx.cr0.eq) goto loc_825817CC;
	// lwz r3,8(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x825817cc
	if (ctx.cr6.eq) goto loc_825817CC;
	// lwz r11,0(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// li r6,0
	ctx.r6.s64 = 0;
	// mr r4,r30
	ctx.r4.u64 = ctx.r30.u64;
	// lwz r5,4(r31)
	ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lwz r11,4(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x825817CC;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_825817CC:
	// addi r30,r30,1
	ctx.r30.s64 = ctx.r30.s64 + 1;
	// cmpwi cr6,r30,20
	ctx.cr6.compare<int32_t>(ctx.r30.s32, 20, ctx.xer);
	// blt cr6,0x82581798
	if (ctx.cr6.lt) goto loc_82581798;
	// addi r3,r1,96
	ctx.r3.s64 = ctx.r1.s64 + 96;
	// bl 0x82580870
	ctx.lr = 0x825817E0;
	sub_82580870(ctx, base);
	// addi r11,r31,12
	ctx.r11.s64 = ctx.r31.s64 + 12;
	// mr r4,r3
	ctx.r4.u64 = ctx.r3.u64;
	// li r5,288
	ctx.r5.s64 = 288;
	// mr r3,r11
	ctx.r3.u64 = ctx.r11.u64;
	// bl 0x825fc8b0
	ctx.lr = 0x825817F4;
	sub_825FC8B0(ctx, base);
loc_825817F4:
	// lfs f0,408(r31)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(ctx.r31.u32 + 408);
	ctx.f0.f64 = double(temp.f32);
	// fcmpu cr6,f0,f31
	ctx.cr6.compare(ctx.f0.f64, ctx.f31.f64);
	// ble cr6,0x82581880
	if (!ctx.cr6.gt) goto loc_82581880;
	// lis r11,-32255
	ctx.r11.s64 = -2113863680;
	// lfs f13,-26072(r11)
	temp.u32 = REX_LOAD_U32(ctx.r11.u32 + -26072);
	ctx.f13.f64 = double(temp.f32);
	// fsubs f0,f0,f13
	ctx.f0.f64 = double(float(ctx.f0.f64 - ctx.f13.f64));
	// stfs f0,408(r31)
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r31.u32 + 408, temp.u32);
	// fcmpu cr6,f0,f31
	ctx.cr6.compare(ctx.f0.f64, ctx.f31.f64);
	// bgt cr6,0x82581820
	if (ctx.cr6.gt) goto loc_82581820;
	// bso cr6,0x82581820
	if (ctx.cr6.so) goto loc_82581820;
	// stfs f31,408(r31)
	temp.f32 = float(ctx.f31.f64);
	REX_STORE_U32(ctx.r31.u32 + 408, temp.u32);
loc_82581820:
	// lfs f0,408(r31)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(ctx.r31.u32 + 408);
	ctx.f0.f64 = double(temp.f32);
	// lis r11,-32248
	ctx.r11.s64 = -2113404928;
	// lfs f13,404(r31)
	temp.u32 = REX_LOAD_U32(ctx.r31.u32 + 404);
	ctx.f13.f64 = double(temp.f32);
	// addi r10,r1,80
	ctx.r10.s64 = ctx.r1.s64 + 80;
	// fdivs f13,f0,f13
	ctx.f13.f64 = double(float(ctx.f0.f64 / ctx.f13.f64));
	// lfs f12,396(r31)
	temp.u32 = REX_LOAD_U32(ctx.r31.u32 + 396);
	ctx.f12.f64 = double(temp.f32);
	// lfs f11,400(r31)
	temp.u32 = REX_LOAD_U32(ctx.r31.u32 + 400);
	ctx.f11.f64 = double(temp.f32);
	// addi r4,r1,80
	ctx.r4.s64 = ctx.r1.s64 + 80;
	// lwz r3,4(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
	// lfs f0,-9532(r11)
	temp.u32 = REX_LOAD_U32(ctx.r11.u32 + -9532);
	ctx.f0.f64 = double(temp.f32);
	// stw r28,0(r10)
	REX_STORE_U32(ctx.r10.u32 + 0, ctx.r28.u32);
	// fmuls f12,f12,f13
	ctx.f12.f64 = double(float(ctx.f12.f64 * ctx.f13.f64));
	// fmuls f13,f11,f13
	ctx.f13.f64 = double(float(ctx.f11.f64 * ctx.f13.f64));
	// fmuls f12,f12,f0
	ctx.f12.f64 = double(float(ctx.f12.f64 * ctx.f0.f64));
	// fmuls f0,f13,f0
	ctx.f0.f64 = double(float(ctx.f13.f64 * ctx.f0.f64));
	// fctidz f13,f12
	ctx.f13.s64 = std::isnan(ctx.f12.f64) ? int64_t(0x8000000000000000ULL) : (ctx.f12.f64 > double(LLONG_MAX)) ? LLONG_MAX : simde_mm_cvttsd_si64(simde_mm_load_sd(&ctx.f12.f64));
	// stfd f13,88(r1)
	REX_STORE_U64(ctx.r1.u32 + 88, ctx.f13.u64);
	// lhz r11,94(r1)
	ctx.r11.u64 = REX_LOAD_U16(ctx.r1.u32 + 94);
	// fctidz f0,f0
	ctx.f0.s64 = std::isnan(ctx.f0.f64) ? int64_t(0x8000000000000000ULL) : (ctx.f0.f64 > double(LLONG_MAX)) ? LLONG_MAX : simde_mm_cvttsd_si64(simde_mm_load_sd(&ctx.f0.f64));
	// stfd f0,88(r1)
	REX_STORE_U64(ctx.r1.u32 + 88, ctx.f0.u64);
	// lhz r10,94(r1)
	ctx.r10.u64 = REX_LOAD_U16(ctx.r1.u32 + 94);
	// sth r11,80(r1)
	REX_STORE_U16(ctx.r1.u32 + 80, ctx.r11.u16);
	// sth r10,82(r1)
	REX_STORE_U16(ctx.r1.u32 + 82, ctx.r10.u16);
	// bl 0x822cb598
	ctx.lr = 0x82581880;
	sub_822CB598(ctx, base);
loc_82581880:
	// addi r1,r1,464
	ctx.r1.s64 = ctx.r1.s64 + 464;
	// lfd f29,-80(r1)
	ctx.fpscr.disableFlushMode();
	ctx.f29.u64 = REX_LOAD_U64(ctx.r1.u32 + -80);
	// lfd f30,-72(r1)
	ctx.f30.u64 = REX_LOAD_U64(ctx.r1.u32 + -72);
	// lfd f31,-64(r1)
	ctx.f31.u64 = REX_LOAD_U64(ctx.r1.u32 + -64);
	// b 0x825fc880
	__restgprlr_26(ctx, base);
    After(poll_before, base);
	return;
}
