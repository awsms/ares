#pragma once

#include "../../desktop-ui.hpp"
#include "../retroachievements.hpp"

#if ARES_ENABLE_RCHEEVOS

namespace RA::Platform::Detail {

struct Adapter {
  // Base contract for per-system RetroAchievements integration.
  // Each adapter maps emulator state to rcheevos console ID, memory view, and ROM/hash input.
  virtual ~Adapter() = default;
  virtual auto supports(const Emulator& emu) const -> bool = 0;
  virtual auto consoleId(const Emulator& emu) const -> u32 = 0;
  virtual auto debugMemoryProfile() const -> RA::Debug::MemoryProfile { return RA::Debug::MemoryProfile::Default; }
  virtual auto filePath(const Emulator& emu) const -> string {
    if(!emu.game) return {};
    return emu.game->location;
  }
  // Reads from the adapter's RetroAchievements-visible memory map.
  // Implementations should return the actual byte count read, which may be shorter than requested.
  virtual auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 = 0;
  virtual auto romData(const Emulator& emu, std::vector<u8>& out) const -> bool {
    return RA::readProgramRom(emu, out);
  }

protected:
  struct MemoryNodeBinding {
    const char* name = nullptr;
    ares::Node::Debugger::Memory* slot = nullptr;
  };

  enum class ReadMode : u8 {
    Linear,
    LinearSwapped,
    Wrapped,
    WrappedSwapped,
  };

  struct Region {
    u32 start = 0;
    u32 end = 0;
    ReadMode mode = ReadMode::Linear;
    u32 span = 0;
    const ares::Node::Debugger::Memory* primary = nullptr;
    const ares::Node::Debugger::Memory* fallback = nullptr;
  };

  static auto supportsAnyName(const Emulator& emu, std::initializer_list<const char*> names) -> bool;
  static auto refreshMemoryNodeCache(
    ares::Node::Object& rootNode,
    const ares::Node::System* currentRoot,
    std::initializer_list<MemoryNodeBinding> bindings
  ) -> bool;
  // Shared memory-reader backend for adapter helper variants.
  // - Linear modes clamp to [offset, span) and may short-read at the end.
  // - Wrapped modes always return size bytes by modulo-wrapping over span.
  // - Swapped modes apply addr^1 byte-lane selection (used by 16-bit bus layouts).
  static auto readFromMemoryMode(
    const ares::Node::Debugger::Memory& memory, u32 offset, u8* buffer, u32 size, u32 maxSpan, ReadMode mode
  ) -> u32;

  template<typename ReadFn>
  static auto readLinearRegion(u32 offset, u8* buffer, u32 size, u32 span, ReadFn&& readByte) -> u32 {
    if(!buffer || size == 0 || span == 0 || offset >= span) return 0;
    auto readable = size;
    if(offset + readable > span) readable = span - offset;
    for(u32 index : range(readable)) {
      buffer[index] = readByte(offset + index);
    }
    return readable;
  }

  // Linear read helper with end-clamping (no wrapping, no byte-lane swap).
  static auto readFromMemory(const ares::Node::Debugger::Memory& memory, u32 offset, u8* buffer, u32 size, u32 maxSpan) -> u32;
  // Linear read helper with end-clamping and addr^1 byte-lane swap.
  static auto readFromMemorySwapped(const ares::Node::Debugger::Memory& memory, u32 offset, u8* buffer, u32 size, u32 maxSpan) -> u32;
  // Wrapped read helper (modulo over full memory span, no byte-lane swap).
  static auto readFromMemoryWrapped(const ares::Node::Debugger::Memory& memory, u32 offset, u8* buffer, u32 size) -> u32;
  // Wrapped read helper with modulo addressing and addr^1 byte-lane swap.
  static auto readFromMemoryWrappedSwapped(const ares::Node::Debugger::Memory& memory, u32 offset, u8* buffer, u32 size) -> u32;
  // Reads one declarative region mapping from RA address space into a debugger memory node.
  static auto readRegion(const Region& region, u32 address, u8* buffer, u32 size) -> u32;
  // Tries each declarative region in order and returns the first successful read.
  // The first region that can satisfy the request wins, which lets adapters express
  // fallback save-RAM layouts without handwritten dispatch code.
  static auto readRegions(std::initializer_list<Region> regions, u32 address, u8* buffer, u32 size) -> u32;
};

// Convenience base for adapters with a compile-time fixed rcheevos console ID.
// Adapters that switch ID by runtime variant (ex: MD/CD/32X) should inherit Adapter directly.
template<u32 ConsoleId>
struct FixedConsoleAdapter : Adapter {
  auto consoleId(const Emulator& emu) const -> u32 override {
    (void)emu;
    return ConsoleId;
  }
};

auto registerNintendoAdapters(std::vector<Adapter*>& list) -> void;
auto registerSegaAdapters(std::vector<Adapter*>& list) -> void;
auto registerSonyAdapters(std::vector<Adapter*>& list) -> void;
auto registerNecAdapters(std::vector<Adapter*>& list) -> void;
auto registerSnkAdapters(std::vector<Adapter*>& list) -> void;
auto registerMiscAdapters(std::vector<Adapter*>& list) -> void;

auto adapters() -> std::vector<Adapter*>&;
auto selectAdapter(const Emulator& emu) -> Adapter*;

}

#endif
