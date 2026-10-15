#pragma once

namespace pvz {
// Called by the game thread on every present. With pvz_hitch_profile, a
// sampler logs where that thread spends any stall longer than 50 ms.
void NotePresent();
void StopHitchProfiler();
}
