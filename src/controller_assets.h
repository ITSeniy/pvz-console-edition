#pragma once
struct PPCContext;
namespace pvz {
void PublishSonyController(bool sony);
void UpdateControllerAssets(PPCContext& ctx, unsigned char* base);
unsigned int ResolveControllerImage(unsigned int image, unsigned char* base);
unsigned int MouseRmbImage();
}
