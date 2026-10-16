#pragma once
#include <string>
#include <string_view>

namespace pvz {
// The game draws a 1280x720 frame. The SDK can render it at a whole multiple
// of that size only (resolution_scale): its EDRAM resolve and texture
// addressing work in whole guest pixels. The presenter then scales the frame
// to the window, so "auto" renders at least as tall as the output and only
// ever scales down: 1080p renders 2560x1440, 4K renders 3840x2160.
int ResolveRenderScale(std::string_view setting, int output_height);
// Before the GPU starts: picks the scale from pvz_render_scale ("auto" or
// 1-4) and the display the game opens on, and sets resolution_scale.
void ApplyRenderScale();
// The scale this launch renders at, and the pvz_render_scale it came from.
int RunningRenderScale();
std::string RunningRenderScaleSetting();
}
