#include "../adapter.hpp"

extern "C" {
#include <rcheevos/rcheevos/include/rc_consoles.h>
}

#if ARES_ENABLE_RCHEEVOS

namespace {

using namespace RA::Platform::Detail;

#if defined(CORE_PS1)
struct PlayStationAdapter final : FixedConsoleAdapter<RC_CONSOLE_PLAYSTATION> {
  mutable ares::Node::Object rootNode;
  mutable ares::Node::Debugger::Memory cpuRamMemory;
  mutable ares::Node::Debugger::Memory cpuScratchpadMemory;

  auto refreshMemoryNodes() const -> void {
    refreshMemoryNodeCache(rootNode, emulator ? &emulator->root : nullptr, {
      {"CPU RAM", &cpuRamMemory},
      {"CPU Scratchpad", &cpuScratchpadMemory},
    });
  }

  auto supports(const Emulator& emu) const -> bool override {
    return supportsAnyName(emu, {"PlayStation"});
  }

  auto debugMemoryProfile() const -> RA::Debug::MemoryProfile override {
    return RA::Debug::MemoryProfile::PlayStation;
  }

  auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 override {
    refreshMemoryNodes();
    // rcheevos PlayStation map:
    // 0x000000-0x1fffff -> Main RAM (2MB)
    // 0x200000-0x2003ff -> Scratchpad RAM (1KB)
    return Adapter::readRegions({
      {0x000000, 0x1fffff, ReadMode::Linear, 0x200000, &cpuRamMemory},
      {0x200000, 0x2003ff, ReadMode::Linear, 0x400, &cpuScratchpadMemory},
    }, address, buffer, size);
  }
};
#endif

}

namespace RA::Platform::Detail {

auto registerSonyAdapters(std::vector<Adapter*>& list) -> void {
#if defined(CORE_PS1)
  static PlayStationAdapter playStation;
  list.push_back(&playStation);
#endif
}

}

#endif
