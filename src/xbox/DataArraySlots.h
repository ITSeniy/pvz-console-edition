// Xbox DataArray slots include an allocation ID after the object payload.
// Allocators 8214D8F0, 8214DA40, 8214DCE0 prove these strides and ID offsets.
#pragma once
#include "Plant.h"
#include "Zombie.h"
#include "Coin.h"

namespace pvz::xbox {
struct PlantSlot { Plant object; rex::be<uint32_t> id; };
struct ZombieSlot { Zombie object; rex::be<uint32_t> id; };
struct CoinSlot { Coin object; rex::be<uint32_t> id; };
static_assert(offsetof(PlantSlot, object) == 0);
static_assert(offsetof(PlantSlot, id) == 0x168);
static_assert(sizeof(PlantSlot) == 0x16C);
static_assert(offsetof(ZombieSlot, object) == 0);
static_assert(offsetof(ZombieSlot, id) == 0x180);
static_assert(sizeof(ZombieSlot) == 0x184);
static_assert(offsetof(CoinSlot, object) == 0);
static_assert(offsetof(CoinSlot, id) == 0xC4);
static_assert(sizeof(CoinSlot) == 0xC8);
} // namespace pvz::xbox
