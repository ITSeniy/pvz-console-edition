// SDK 0.10.0's distributed runtime omits the Vision camera imports used here.
// PvZ must see a disconnected camera; no successful frame capture is claimed.
#include <rex/hook.h>

REX_HOOK_RAW(__imp__XUsbcamCreate) { ctx.r3.u64 = 0; }
REX_HOOK_RAW(__imp__XUsbcamDestroy) { ctx.r3.u64 = 0; }
REX_HOOK_RAW(__imp__XUsbcamGetState) { ctx.r3.u64 = 0; }
REX_HOOK_RAW(__imp__XUsbcamSetConfig) { ctx.r3.u64 = 1167; }
REX_HOOK_RAW(__imp__XUsbcamSetView) { ctx.r3.u64 = 1167; }
REX_HOOK_RAW(__imp__XUsbcamSetCaptureMode) { ctx.r3.u64 = 1167; }
REX_HOOK_RAW(__imp__XUsbcamReadFrame) { ctx.r3.u64 = 1167; }
