#include "../adapter.hpp"

extern "C" {
#include <rcheevos/rcheevos/include/rc_consoles.h>
}

#if ARES_ENABLE_RCHEEVOS

namespace {

using namespace RA::Platform::Detail;

#if defined(CORE_CV)
struct ColecoVisionAdapter final : FixedConsoleAdapter<RC_CONSOLE_COLECOVISION> {
  mutable ares::Node::Object rootNode;
  mutable ares::Node::Debugger::Memory cpuRamMemory;
  mutable ares::Node::Debugger::Memory cpuExpansionMemory;

  auto refreshMemoryNodes() const -> void {
    refreshMemoryNodeCache(rootNode, emulator ? &emulator->root : nullptr, {
      {"CPU RAM", &cpuRamMemory},
      {"CPU EXPRAM", &cpuExpansionMemory},
    });
  }

  auto supports(const Emulator& emu) const -> bool override {
    return supportsAnyName(emu, {"ColecoVision"});
  }

  auto debugMemoryProfile() const -> RA::Debug::MemoryProfile override {
    return RA::Debug::MemoryProfile::ColecoVision;
  }

  auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 override {
    if(!buffer || size == 0) return 0;
    refreshMemoryNodes();
    // rcheevos ColecoVision map:
    // 0x000000-0x0003ff -> System RAM (1KB)
    // 0x000400-0x0023ff -> SGM low RAM (virtual)
    // 0x002400-0x0083ff -> SGM high RAM (virtual)
    if(address > 0x83ff) return 0;
    auto readable = size;
    if(address + readable > 0x8400) readable = 0x8400 - address;

    for(u32 index : range(readable)) {
      auto virtualAddress = address + index;
      if(virtualAddress <= 0x03ff) {
        if(!cpuRamMemory || virtualAddress >= cpuRamMemory->size()) return index;
        buffer[index] = cpuRamMemory->read(virtualAddress);
        continue;
      }

      if(!cpuExpansionMemory) return index;
      auto expansionSize = cpuExpansionMemory->size();
      if(expansionSize == 0) return index;

      u32 expansionAddress = 0;
      if(virtualAddress <= 0x23ff) expansionAddress = virtualAddress - 0x0400;
      else expansionAddress = 0x2000 + (virtualAddress - 0x2400);
      buffer[index] = cpuExpansionMemory->read(expansionAddress % expansionSize);
    }
    return readable;
  }
};
#endif

#if defined(CORE_WS)
struct WonderSwanAdapter final : FixedConsoleAdapter<RC_CONSOLE_WONDERSWAN> {
  mutable ares::Node::Object rootNode;
  mutable ares::Node::Debugger::Memory cpuRamMemory;
  mutable ares::Node::Debugger::Memory cartridgeRamMemory;
  mutable ares::Node::Debugger::Memory cartridgeEepromMemory;

  auto refreshMemoryNodes() const -> void {
    refreshMemoryNodeCache(rootNode, emulator ? &emulator->root : nullptr, {
      {"CPU RAM", &cpuRamMemory},
      {"Cartridge RAM", &cartridgeRamMemory},
      {"Cartridge EEPROM", &cartridgeEepromMemory},
    });
  }

  auto supports(const Emulator& emu) const -> bool override {
    return supportsAnyName(emu, {"WonderSwan", "WonderSwan Color", "Pocket Challenge V2"});
  }

  auto debugMemoryProfile() const -> RA::Debug::MemoryProfile override {
    return RA::Debug::MemoryProfile::WonderSwan;
  }

  auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 override {
    refreshMemoryNodes();
    // rcheevos WonderSwan map:
    // 0x000000-0x00ffff -> system RAM (16KB mono, 64KB color)
    // 0x010000-0x08ffff -> cartridge save RAM / EEPROM
    return Adapter::readRegions({
      {0x000000, 0x00ffff, ReadMode::Linear, 0x10000, &cpuRamMemory},
      {0x010000, 0x08ffff, ReadMode::Linear, 0x80000, &cartridgeRamMemory, &cartridgeEepromMemory},
    }, address, buffer, size);
  }
};
#endif

}

namespace RA::Platform::Detail {

auto registerMiscAdapters(std::vector<Adapter*>& list) -> void {
#if defined(CORE_CV)
  static ColecoVisionAdapter colecoVision;
  list.push_back(&colecoVision);
#endif
#if defined(CORE_WS)
  static WonderSwanAdapter wonderSwan;
  list.push_back(&wonderSwan);
#endif
}

}

#endif
