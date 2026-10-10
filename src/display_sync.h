#pragma once
namespace rex::ui { class GraphicsProvider; }
namespace pvz {
bool InstallDisplaySync(rex::ui::GraphicsProvider*, void* window, bool enabled);
void RemoveDisplaySync();
}
