// Explicit integration points for existing port hooks. Diagnostics are disabled
// by default; none of these helpers changes the incoming guest context.
#pragma once
#include <cstdint>

struct PPCContext;
namespace pvz::native {
bool RenderTracing();
int64_t RenderClockSeconds();
// The caller returns the scheduler's pending-tick result when this is true.
bool HoldRenderUpdate();
// Call at hook entry, before any port-specific argument adaptation.
void TraceGraphics(uint32_t address, const PPCContext& context, uint8_t* base);
}
