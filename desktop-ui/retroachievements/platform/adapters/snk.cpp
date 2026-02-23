#include "../adapter.hpp"

extern "C" {
#include <rcheevos/rcheevos/include/rc_consoles.h>
}

#if ARES_ENABLE_RCHEEVOS

namespace {

using namespace RA::Platform::Detail;

#if defined(CORE_NGP)
struct NeoGeoPocketAdapter final : FixedConsoleAdapter<RC_CONSOLE_NEOGEO_POCKET> {
  mutable ares::Node::Object rootNode;
  mutable ares::Node::Debugger::Memory cpuRamMemory;
  mutable ares::Node::Debugger::Memory apuRamMemory;

  auto refreshMemoryNodes() const -> void {
    refreshMemoryNodeCache(rootNode, emulator ? &emulator->root : nullptr, {
      {"CPU RAM", &cpuRamMemory},
      {"APU RAM", &apuRamMemory},
    });
  }

  auto supports(const Emulator& emu) const -> bool override {
    return supportsAnyName(emu, {"Neo Geo Pocket", "Neo Geo Pocket Color"});
  }

  auto debugMemoryProfile() const -> RA::Debug::MemoryProfile override {
    return RA::Debug::MemoryProfile::NeoGeoPocket;
  }

  auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 override {
    if(!buffer || size == 0) return 0;
    refreshMemoryNodes();
    // rcheevos NGP map:
    // 0x0000-0x3fff -> 0x4000-0x7fff (CPU RAM + APU RAM window)
    if(address > 0x3fff) return 0;
    auto readable = size;
    if(address + readable > 0x4000) readable = 0x4000 - address;
    for(u32 index : range(readable)) {
      auto mapped = 0x4000 + address + index;
      if(mapped <= 0x6fff) {
        if(!cpuRamMemory) return index;
        auto offset = mapped - 0x4000;
        if(offset >= cpuRamMemory->size()) return index;
        buffer[index] = cpuRamMemory->read(offset);
      } else {
        if(!apuRamMemory) return index;
        auto offset = mapped - 0x7000;
        if(offset >= apuRamMemory->size()) return index;
        buffer[index] = apuRamMemory->read(offset);
      }
    }
    return readable;
  }
};
#endif

#if defined(CORE_NG)
struct NeoGeoAdapter final : FixedConsoleAdapter<RC_CONSOLE_ARCADE> {
  mutable ares::Node::Object rootNode;
  mutable ares::Node::Debugger::Memory systemWorkRamMemory;
  mutable ares::Node::Debugger::Memory systemBackupRamMemory;

  auto refreshMemoryNodes() const -> bool {
    if(systemWorkRamMemory) return true;
    if(!refreshMemoryNodeCache(rootNode, emulator ? &emulator->root : nullptr, {
      {"System Work RAM", &systemWorkRamMemory},
      {"System Backup RAM", &systemBackupRamMemory},
    })) return false;
    return (bool)systemWorkRamMemory;
  }

  auto supports(const Emulator& emu) const -> bool override {
    return supportsAnyName(emu, {"Neo Geo AES", "Neo Geo MVS"});
  }

  auto debugMemoryProfile() const -> RA::Debug::MemoryProfile override {
    return RA::Debug::MemoryProfile::NeoGeo;
  }

  auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 override {
    if(!refreshMemoryNodes()) return 0;
    // Match libretro behavior for Neo Geo:
    // 0x00000-0x0ffff: 68K RAM, byte-lane swapped for host endianness.
    // 0x10000-0x1ffff: MVS backup RAM (when available).
    return Adapter::readRegions({
      {0x000000, 0x00ffff, ReadMode::LinearSwapped, 0x10000, &systemWorkRamMemory},
      {0x010000, 0x01ffff, ReadMode::LinearSwapped, 0x10000, &systemBackupRamMemory},
    }, address, buffer, size);
  }
};
#endif

}

namespace RA::Platform::Detail {

auto registerSnkAdapters(std::vector<Adapter*>& list) -> void {
#if defined(CORE_NGP)
  static NeoGeoPocketAdapter neoGeoPocket;
  list.push_back(&neoGeoPocket);
#endif
#if defined(CORE_NG)
  static NeoGeoAdapter neoGeo;
  list.push_back(&neoGeo);
#endif
}

}

#endif
