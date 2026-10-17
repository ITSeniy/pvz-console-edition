// Compile-time ABI checks only. This test never maps or launches the game.
#include "xbox/LawnApp.h"
#include "xbox/Board.h"
#include "xbox/BaseGamepadControls.h"
#include "xbox/GamepadControls.h"
#include "xbox/SeedBank.h"
#include "xbox/SeedPacket.h"
#include "xbox/Plant.h"
#include "xbox/Zombie.h"
#include "xbox/Coin.h"
#include "xbox/Challenge.h"
#include "xbox/CursorObject.h"
#include "xbox/Widget.h"
#include "xbox/DataArraySlots.h"

using namespace pvz::xbox;
static_assert(std::is_same_v<GuestPtr, rex::be<uint32_t>>);
static_assert(sizeof(SeedBank::packets) / sizeof(SeedPacket) == 10);
static_assert(offsetof(SeedBank, packets) + 9 * sizeof(SeedPacket) == 0x4AC);
static_assert(offsetof(Board, seed_banks) + sizeof(GuestPtr) == 0x1A8);
static_assert(offsetof(Board, controls) + sizeof(GuestPtr) == 0x1B4);
static_assert(offsetof(Board, cursors) + sizeof(GuestPtr) == 0x1BC);
static_assert(offsetof(Board, sun) + sizeof(rex::be<int32_t>) == 0x55F8);
int main() { return 0; }
