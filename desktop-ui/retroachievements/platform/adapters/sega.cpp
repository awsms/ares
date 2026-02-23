#include "../adapter.hpp"

#if defined(CORE_SG)
  #include <sg/sg.hpp>
#endif

extern "C" {
#include <rcheevos/rcheevos/include/rc_consoles.h>
}

#if ARES_ENABLE_RCHEEVOS

namespace {

using namespace RA::Platform::Detail;

struct Sega8BitAdapter final : Adapter {
  mutable ares::Node::Object rootNode;
  mutable ares::Node::Debugger::Memory cpuRamMemory;
  mutable bool sg1000Mode = false;

  auto ensureCpuRamMemory() const -> bool {
    if(cpuRamMemory) return true;
    if(!refreshMemoryNodeCache(rootNode, emulator ? &emulator->root : nullptr, {{"CPU RAM", &cpuRamMemory}})) return false;
    return (bool)cpuRamMemory;
  }

  auto supports(const Emulator& emu) const -> bool override {
    sg1000Mode = (emu.name == "SG-1000");
    return supportsAnyName(emu, {"SG-1000", "Master System", "Game Gear"});
  }

  auto consoleId(const Emulator& emu) const -> u32 override {
    if(emu.name == "SG-1000") return RC_CONSOLE_SG1000;
    if(emu.name == "Game Gear") return RC_CONSOLE_GAME_GEAR;
    return RC_CONSOLE_MASTER_SYSTEM;
  }

  auto debugMemoryProfile() const -> RA::Debug::MemoryProfile override {
    if(sg1000Mode) return RA::Debug::MemoryProfile::Sega8BitSG1000;
    return RA::Debug::MemoryProfile::Sega8Bit;
  }

  auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 override {
    if(!buffer || size == 0) return 0;

    if(sg1000Mode) {
      // Match rcheevos SG-1000 map:
      // 0x0000-0x03ff -> $c000 (base RAM)
      // 0x0400-0x1fff -> $c400 (expansion mode B)
      // 0x2000-0x3fff -> $2000 (expansion mode A)
      // 0x4000-0x5fff -> $8000 (cart expansion RAM)
      // Unmapped regions will naturally read open-bus/ROM values from the core.
      if(address > 0x5fff) return 0;
      auto readable = size;
      if(address + readable > 0x6000) readable = 0x6000 - address;
#if defined(CORE_SG)
      for(u32 index : range(readable)) {
        auto virtualAddress = address + index;
        u32 mapped = 0;
        if(virtualAddress <= 0x03ff) mapped = 0xc000 + virtualAddress;
        else if(virtualAddress <= 0x1fff) mapped = 0xc400 + (virtualAddress - 0x0400);
        else if(virtualAddress <= 0x3fff) mapped = 0x2000 + (virtualAddress - 0x2000);
        else mapped = 0x8000 + (virtualAddress - 0x4000);
        buffer[index] = ares::SG1000::cpu.read((n16)mapped);
      }
      return readable;
#else
      return 0;
#endif
    }

    if(!ensureCpuRamMemory()) return 0;
    auto ramSize = cpuRamMemory->size();
    if(ramSize == 0) return 0;
    // Match rcheevos SMS/GG RAM exposure:
    // 0x0000-0x1fff system RAM (mirrored), 0x2000-0x9fff cartridge RAM.
    if(address <= 0x1fff) {
      auto readable = size;
      if(address + readable > 0x2000) readable = 0x2000 - address;
      for(u32 index : range(readable)) {
        auto mapped = (address + index) & 0x1fff;
        if(mapped >= ramSize) mapped %= ramSize;
        buffer[index] = cpuRamMemory->read(mapped);
      }
      return readable;
    }

    return 0;
  }
};

#if defined(CORE_MD)
struct MegaDriveAdapter final : Adapter {
  enum class RuntimeMode : u8 { MegaDrive, MegaCd, Mega32X };

  mutable ares::Node::Object rootNode;
  mutable ares::Node::Debugger::Memory cpuRamMemory;
  mutable ares::Node::Debugger::Memory cdPramMemory;
  mutable ares::Node::Debugger::Memory cdWramMemory;
  mutable ares::Node::Debugger::Memory m32xSdramMemory;
  mutable ares::Node::Debugger::Memory m32xDramMemory;
  mutable ares::Node::Debugger::Memory cartridgeRamMemory;
  mutable RuntimeMode runtimeMode = RuntimeMode::MegaDrive;

  auto refreshMemoryNodes() const -> void {
    refreshMemoryNodeCache(rootNode, emulator ? &emulator->root : nullptr, {
      {"CPU RAM", &cpuRamMemory},
      {"CD PRAM", &cdPramMemory},
      {"CD WRAM", &cdWramMemory},
      {"32X SDRAM", &m32xSdramMemory},
      {"32X DRAM", &m32xDramMemory},
      {"Cartridge RAM", &cartridgeRamMemory},
    });
  }

  auto read32xRam(u32 offset, u8* buffer, u32 size) const -> u32 {
    // Prefer SH2 SDRAM (the official rcheevos 32X RAM source), but gracefully
    // fall back to the VDP DRAM node when the SDRAM node is missing.
    auto read = Adapter::readFromMemorySwapped(m32xSdramMemory, offset, buffer, size, 0x40000);
    if(read) return read;
    return Adapter::readFromMemorySwapped(m32xDramMemory, offset, buffer, size, 0x40000);
  }

  auto supports(const Emulator& emu) const -> bool override {
    if(emu.name == "Mega 32X") {
      runtimeMode = RuntimeMode::Mega32X;
      return true;
    }
    if(emu.name == "Mega CD" || emu.name == "LaserActive (SEGA PAC)") {
      runtimeMode = RuntimeMode::MegaCd;
      return true;
    }
    if(emu.name == "Mega Drive") {
      runtimeMode = RuntimeMode::MegaDrive;
      return true;
    }
    return false;
  }

  auto consoleId(const Emulator& emu) const -> u32 override {
    if(emu.name == "Mega 32X") return RC_CONSOLE_SEGA_32X;
    if(emu.name == "Mega CD" || emu.name == "LaserActive (SEGA PAC)") return RC_CONSOLE_SEGA_CD;
    return RC_CONSOLE_MEGA_DRIVE;
  }

  auto debugMemoryProfile() const -> RA::Debug::MemoryProfile override {
    if(runtimeMode == RuntimeMode::Mega32X) return RA::Debug::MemoryProfile::Mega32X;
    if(runtimeMode == RuntimeMode::MegaCd) return RA::Debug::MemoryProfile::MegaCd;
    return RA::Debug::MemoryProfile::MegaDrive;
  }

  auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 override {
    refreshMemoryNodes();

    if(runtimeMode == RuntimeMode::Mega32X) {
      // libretro MD/32X cores expose byte-addressed RAM with word-swapped
      // storage layout (addr^1 on little-endian hosts). Mirror that layout
      // here so existing 32X achievements evaluate against expected bytes.
      // rcheevos Genesis 32X map:
      // 0x000000-0x00ffff -> MD RAM
      // 0x010000-0x04ffff -> 32X RAM
      // 0x050000-0x05ffff -> Cartridge RAM
      if(address <= 0x00ffff) {
        return Adapter::readRegions({
          {0x000000, 0x00ffff, ReadMode::LinearSwapped, 0x10000, &cpuRamMemory},
        }, address, buffer, size);
      }
      if(address >= 0x010000 && address <= 0x04ffff) return read32xRam(address - 0x010000, buffer, size);
      if(address >= 0x050000 && address <= 0x05ffff) {
        return Adapter::readRegions({
          {0x050000, 0x05ffff, ReadMode::WrappedSwapped, 0, &cartridgeRamMemory},
        }, address, buffer, size);
      }
      return 0;
    }

    if(runtimeMode == RuntimeMode::MegaCd) {
      // Match libretro-style byte lane layout used by existing Sega CD sets.
      // rcheevos Sega CD map:
      // 0x000000-0x00ffff -> 68000 RAM
      // 0x010000-0x08ffff -> CD PRG RAM
      // 0x090000-0x0affff -> CD WORD RAM
      return Adapter::readRegions({
        {0x000000, 0x00ffff, ReadMode::LinearSwapped, 0x10000, &cpuRamMemory},
        {0x010000, 0x08ffff, ReadMode::LinearSwapped, 0x80000, &cdPramMemory},
        {0x090000, 0x0affff, ReadMode::LinearSwapped, 0x20000, &cdWramMemory},
      }, address, buffer, size);
    }

    // rcheevos Mega Drive map:
    // 0x000000-0x00ffff -> MD RAM
    // 0x010000-0x01ffff -> Cartridge RAM
    // rcheevos expects the libretro-style 68K byte lane view (addr^1 on LE
    // hosts for MD RAM). Mirror that layout here so achievement conditions
    // evaluate against the expected bytes.
    return Adapter::readRegions({
      {0x000000, 0x00ffff, ReadMode::LinearSwapped, 0x10000, &cpuRamMemory},
      {0x010000, 0x01ffff, ReadMode::WrappedSwapped, 0, &cartridgeRamMemory},
    }, address, buffer, size);
  }
};
#endif

}

namespace RA::Platform::Detail {

auto registerSegaAdapters(std::vector<Adapter*>& list) -> void {
#if defined(CORE_SG) || defined(CORE_MS)
  static Sega8BitAdapter sega8Bit;
  list.push_back(&sega8Bit);
#endif
#if defined(CORE_MD)
  static MegaDriveAdapter megaDrive;
  list.push_back(&megaDrive);
#endif
}

}

#endif
