// Xbox guest layout vocabulary. Endian storage comes from the pinned SDK.
#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <rex/types.h>

namespace pvz::xbox {
// Stored address only. Resolve through guest memory; never reinterpret as a host pointer.
using GuestPtr = rex::be<uint32_t>;
static_assert(sizeof(GuestPtr) == 4);
static_assert(sizeof(rex::be<int32_t>) == 4);
static_assert(sizeof(rex::be<float>) == 4);
static_assert(sizeof(rex::be<double>) == 8);
static_assert(std::is_standard_layout_v<GuestPtr>);
} // namespace pvz::xbox
