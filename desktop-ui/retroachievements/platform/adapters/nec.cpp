#include "../adapter.hpp"

extern "C" {
#include <rcheevos/rcheevos/include/rc_consoles.h>
}

#if ARES_ENABLE_RCHEEVOS

namespace {

using namespace RA::Platform::Detail;

#if defined(CORE_PCE)
struct PCEngineAdapter final : Adapter {
  mutable ares::Node::Object rootNode;
  mutable ares::Node::Debugger::Memory cpuRamMemory;
  mutable ares::Node::Debugger::Memory cdWramMemory;
  mutable ares::Node::Debugger::Memory cdSramMemory;
  mutable ares::Node::Debugger::Memory cdBramMemory;

  auto refreshMemoryNodes() const -> void {
    refreshMemoryNodeCache(rootNode, emulator ? &emulator->root : nullptr, {
      {"CPU RAM", &cpuRamMemory},
      {"CD WRAM", &cdWramMemory},
      {"CD SRAM", &cdSramMemory},
      {"CD BRAM", &cdBramMemory},
    });
  }

  auto supports(const Emulator& emu) const -> bool override {
    return supportsAnyName(emu, {
      "PC Engine", "PC Engine CD", "PC Engine LD", "SuperGrafx", "SuperGrafx CD", "LaserActive (NEC PAC)"
    });
  }

  auto consoleId(const Emulator& emu) const -> u32 override {
    if(emu.name == "PC Engine CD" || emu.name == "PC Engine LD" || emu.name == "SuperGrafx CD" || emu.name == "LaserActive (NEC PAC)") {
      return RC_CONSOLE_PC_ENGINE_CD;
    }
    return RC_CONSOLE_PC_ENGINE;
  }

  auto debugMemoryProfile() const -> RA::Debug::MemoryProfile override {
    return RA::Debug::MemoryProfile::PCEngine;
  }

  auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 override {
    refreshMemoryNodes();
    // rcheevos PC Engine maps:
    // 0x000000-0x001fff: base system RAM (8KB)
    // 0x002000-0x011fff: CD RAM (64KB)
    // 0x012000-0x041fff: Super System Card RAM (192KB)
    // 0x042000-0x0427ff: CD BRAM (2KB)
    return Adapter::readRegions({
      {0x000000, 0x001fff, ReadMode::Linear, 0x2000, &cpuRamMemory},
      {0x002000, 0x011fff, ReadMode::Linear, 0x10000, &cdWramMemory},
      {0x012000, 0x041fff, ReadMode::Linear, 0x30000, &cdSramMemory},
      {0x042000, 0x0427ff, ReadMode::Linear, 0x800, &cdBramMemory},
    }, address, buffer, size);
  }
};
#endif

}

namespace RA::Platform::Detail {

auto registerNecAdapters(std::vector<Adapter*>& list) -> void {
#if defined(CORE_PCE)
  static PCEngineAdapter pcEngine;
  list.push_back(&pcEngine);
#endif
}

}

#endif
