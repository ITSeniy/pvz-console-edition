// Partial Xbox layouts established by the T16 constructor/setter bodies.
#pragma once
#include "Types.h"
#include <cstddef>

namespace pvz::xbox {
struct RefCount {
  GuestPtr vtable;
};
struct DataElement {
  GuestPtr vtable;
  uint8_t field_4;
};
struct DataWriter {
  GuestPtr vtable;
  rex::be<uint32_t> field_4;
  rex::be<uint32_t> field_8;
  rex::be<uint32_t> field_C;
  rex::be<uint32_t> field_10;
};
struct DataReader {
  GuestPtr vtable;
  rex::be<uint32_t> field_4;
  rex::be<uint32_t> field_8;
  rex::be<uint32_t> field_C;
  rex::be<uint32_t> field_10;
  uint8_t field_14;
};
// Checked vector prefix: 82552538, 825535E0, 82556E10/82556EE8.
struct DataVector {
  GuestPtr proxy;
  uint8_t pad_004[8];
  GuestPtr begin;
  GuestPtr end;
  GuestPtr capacity_end;
};
struct SyncObject {
  GuestPtr vtable;
  DataVector vector;
};
struct Buffer {
  GuestPtr vtable;
  DataVector bytes;
  rex::be<uint32_t> field_1C;
  rex::be<uint32_t> field_20;
  rex::be<uint32_t> field_24;
};
struct SingleDataElement {
  DataElement element;
  uint8_t name[0x1C]; // guest string object, passed to 82125760/82125F60
  GuestPtr nested_element;
};
struct ListDataElement {
  DataElement element;
  DataVector elements;
};
struct EncodingParser {
  GuestPtr vtable;
  GuestPtr stream;
  DataVector buffered_characters;
  GuestPtr field_20;
  uint8_t field_24;
  uint8_t first_character;
  uint8_t swap_utf16;
};
struct DataFileCursor {
  uint8_t pad_000[0x10];
  rex::be<uint32_t> length;
  rex::be<uint32_t> position;
};
struct DataString {
  GuestPtr proxy;
  uint8_t storage[16]; // inline bytes, or a guest pointer in the first word
  rex::be<uint32_t> length;
  rex::be<uint32_t> capacity;
};
struct DataSync {
  GuestPtr vtable;
  GuestPtr reader;
  GuestPtr writer;
  uint8_t pad_00C[4];
  uint8_t map_10[0x20]; // opaque map objects; only passed to guest constructors/destructors
  uint8_t map_30[0x20];
  DataVector objects;
};
static_assert(sizeof(RefCount) == 4);
static_assert(offsetof(DataElement, field_4) == 4);
static_assert(offsetof(DataWriter, field_4) == 4);
static_assert(offsetof(DataWriter, field_8) == 8);
static_assert(offsetof(DataWriter, field_C) == 0xC);
static_assert(offsetof(DataWriter, field_10) == 0x10);
static_assert(sizeof(DataWriter) == 0x14);
static_assert(offsetof(DataReader, field_4) == 4);
static_assert(offsetof(DataReader, field_8) == 8);
static_assert(offsetof(DataReader, field_C) == 0xC);
static_assert(offsetof(DataReader, field_10) == 0x10);
static_assert(offsetof(DataReader, field_14) == 0x14);
static_assert(offsetof(EncodingParser, field_20) == 0x20);
static_assert(offsetof(EncodingParser, field_24) == 0x24);
static_assert(sizeof(DataVector) == 0x18);
static_assert(offsetof(DataVector, begin) == 0xC);
static_assert(offsetof(DataVector, end) == 0x10);
static_assert(offsetof(DataVector, capacity_end) == 0x14);
static_assert(offsetof(SyncObject, vector) == 4);
static_assert(offsetof(Buffer, bytes) == 4);
static_assert(offsetof(Buffer, field_1C) == 0x1C);
static_assert(offsetof(Buffer, field_20) == 0x20);
static_assert(offsetof(Buffer, field_24) == 0x24);
static_assert(offsetof(SingleDataElement, name) == 8);
static_assert(offsetof(SingleDataElement, nested_element) == 0x24);
static_assert(offsetof(ListDataElement, elements) == 8);
static_assert(offsetof(EncodingParser, stream) == 4);
static_assert(offsetof(EncodingParser, buffered_characters) == 8);
static_assert(offsetof(EncodingParser, first_character) == 0x25);
static_assert(offsetof(EncodingParser, swap_utf16) == 0x26);
static_assert(offsetof(DataFileCursor, length) == 0x10);
static_assert(offsetof(DataFileCursor, position) == 0x14);
static_assert(offsetof(DataString, storage) == 4);
static_assert(offsetof(DataString, length) == 0x14);
static_assert(offsetof(DataString, capacity) == 0x18);
static_assert(offsetof(DataSync, reader) == 4);
static_assert(offsetof(DataSync, writer) == 8);
static_assert(offsetof(DataSync, map_10) == 0x10);
static_assert(offsetof(DataSync, map_30) == 0x30);
static_assert(offsetof(DataSync, objects) == 0x50);
} // namespace pvz::xbox
