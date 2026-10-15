#pragma once
namespace rex::ui { class GraphicsProvider; }
namespace pvz {
bool InstallDisplaySync(rex::ui::GraphicsProvider*, void* window, bool enabled);
void RemoveDisplaySync();
// The game finished drawing a frame, before the frame clock wait (trace only).
void NoteFrameReady();
// The game submitted a new frame; the next present waits for vertical blank.
void NoteGuestFrame();
// With VSync, waits for the next game frame's display refresh. Returns false
// when the refresh is unavailable and the caller keeps its own frame clock.
bool WaitForDisplayRefresh();
}
