#include "../adapter.hpp"
#include "../fds.hpp"

#if defined(CORE_GB)
  #include <gb/gb.hpp>
#endif
#if defined(CORE_GBA)
  #include <gba/gba.hpp>
#endif

extern "C" {
#include <rcheevos/rcheevos/include/rc_consoles.h>
}

#if ARES_ENABLE_RCHEEVOS

namespace {

using namespace RA::Platform::Detail;

struct GameBoyAdapter final : Adapter {
  auto supports(const Emulator& emu) const -> bool override {
    return supportsAnyName(emu, {"Game Boy", "Game Boy Color"});
  }

  auto consoleId(const Emulator& emu) const -> u32 override {
#if defined(CORE_GB)
    auto systemName = ares::GameBoy::system.name();
    if(systemName == "Game Boy Color") return RC_CONSOLE_GAMEBOY_COLOR;
    if(emu.name == "Game Boy Color") return RC_CONSOLE_GAMEBOY_COLOR;
    return RC_CONSOLE_GAMEBOY;
#else
    (void)emu;
    return RC_CONSOLE_UNKNOWN;
#endif
  }

  auto debugMemoryProfile() const -> RA::Debug::MemoryProfile override {
    return RA::Debug::MemoryProfile::GameBoyFamily;
  }

  auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 override {
#if defined(CORE_GB)
    // rcheevos GB logical map:
    // 0x000000-0x00ffff -> native GB address space
    // 0x010000-0x015fff -> system RAM banks 2-7 on GBC / unused on GB
    // 0x016000-0x033fff -> cartridge RAM banks 1-15
    if(address >= 0x00d000 && address <= 0x00dfff) {
      if(ares::GameBoy::cpu.wram.size() < 0x2000) return 0;
      auto offset = 0x1000 + (address - 0x00d000);
      return readLinearRegion(offset, buffer, size, ares::GameBoy::cpu.wram.size(), [](u32 mapped) -> u8 {
        return ares::GameBoy::cpu.wram[mapped];
      });
    }

    if(address <= 0x00ffff) {
      return readLinearRegion(address, buffer, size, 0x10000, [](u32 absolute) -> u8 {
        return ares::GameBoy::cpu.readDebugger((n16)absolute);
      });
    }

    if(address >= 0x010000 && address <= 0x015fff) {
      if(ares::GameBoy::cpu.wram.size() < 0x8000) return 0;
      auto offset = 0x2000 + (address - 0x010000);
      return readLinearRegion(offset, buffer, size, ares::GameBoy::cpu.wram.size(), [](u32 mapped) -> u8 {
        return ares::GameBoy::cpu.wram[mapped];
      });
    }
#else
    (void)address;
    (void)buffer;
    (void)size;
#endif
    return 0;
  }
};

#if defined(CORE_FC)
struct FamicomAdapter final : Adapter {
  mutable ares::Node::Object rootNode;
  mutable ares::Node::Debugger::Memory cpuRamMemory;

  auto ensureCpuRamMemory() const -> bool {
    if(cpuRamMemory) return true;
    if(!refreshMemoryNodeCache(rootNode, emulator ? &emulator->root : nullptr, {{"CPU RAM", &cpuRamMemory}})) return false;
    return (bool)cpuRamMemory;
  }

  auto supports(const Emulator& emu) const -> bool override {
    return supportsAnyName(emu, {"Famicom", "Famicom Disk System"});
  }

  auto consoleId(const Emulator& emu) const -> u32 override {
    if(emu.name == "Famicom Disk System") return RC_CONSOLE_FAMICOM_DISK_SYSTEM;
    return RC_CONSOLE_NINTENDO;
  }

  auto debugMemoryProfile() const -> RA::Debug::MemoryProfile override {
    return RA::Debug::MemoryProfile::Famicom;
  }

  auto romData(const Emulator& emu, std::vector<u8>& out) const -> bool override {
    out.clear();
    if(!emu.game || !emu.game->pak) return false;

    auto appendPakFile = [&](const char* name) -> bool {
      auto fp = emu.game->pak->read(name);
      if(!fp) return false;
      auto previousSize = out.size();
      out.resize(previousSize + fp->size());
      for(u32 index : range(fp->size())) out[previousSize + index] = fp->read();
      return true;
    };

    if(emu.name == "Famicom Disk System") {
      return RA::Platform::FDS::readCanonicalRomData(emu, out);
    }

    if(emu.name != "Famicom") return false;
    auto hasProgram = appendPakFile("program.rom");
    auto hasCharacter = appendPakFile("character.rom");
    return hasProgram || hasCharacter;
  }

  auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 override {
    if(!buffer || size == 0) return 0;
    if(!ensureCpuRamMemory()) return 0;
    // rcheevos RAM exposure for NES/FDS:
    // 0x0000-0x07ff base RAM, mirrored to 0x1fff.
    if(address > 0x1fff) return 0;
    auto readable = size;
    if(address + readable > 0x2000) readable = 0x2000 - address;
    for(u32 index : range(readable)) {
      buffer[index] = cpuRamMemory->read((address + index) & 0x07ff);
    }
    return readable;
  }
};
#endif

#if defined(CORE_GBA)
struct GameBoyAdvanceAdapter final : FixedConsoleAdapter<RC_CONSOLE_GAMEBOY_ADVANCE> {
  auto supports(const Emulator& emu) const -> bool override {
    return supportsAnyName(emu, {"Game Boy Advance"});
  }

  auto debugMemoryProfile() const -> RA::Debug::MemoryProfile override {
    return RA::Debug::MemoryProfile::GameBoyAdvance;
  }

  auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 override {
#if defined(CORE_GBA)
    if(!buffer || size == 0) return 0;
    // rcheevos GBA logical map:
    // 0x000000-0x007fff -> 0x03000000 (IWRAM)
    // 0x008000-0x047fff -> 0x02000000 (EWRAM)
    // 0x048000-0x057fff -> 0x0e000000 (Game Pak SRAM)
    if(address <= 0x007fff) {
      return readLinearRegion(address, buffer, size, 0x8000, [](u32 mapped) -> u8 {
        return ares::GameBoyAdvance::cpu.iwram[mapped];
      });
    }
    if(address >= 0x008000 && address <= 0x047fff) {
      return readLinearRegion(address - 0x008000, buffer, size, 0x40000, [](u32 mapped) -> u8 {
        return ares::GameBoyAdvance::cpu.ewram[mapped];
      });
    }
    if(address >= 0x048000 && address <= 0x057fff) {
      return readLinearRegion(address - 0x048000, buffer, size, 0x10000, [](u32 mapped) -> u8 {
        return ares::GameBoyAdvance::cartridge.readBackup(mapped);
      });
    }
#else
    (void)address;
    (void)buffer;
    (void)size;
#endif
    return 0;
  }
};
#endif

#if defined(CORE_N64)
struct Nintendo64Adapter final : FixedConsoleAdapter<RC_CONSOLE_NINTENDO_64> {
  mutable ares::Node::Object rootNode;
  mutable ares::Node::Debugger::Memory rdramMemory;

  auto ensureRdramMemory() const -> bool {
    if(rdramMemory) return true;
    if(!refreshMemoryNodeCache(rootNode, emulator ? &emulator->root : nullptr, {{"RDRAM", &rdramMemory}})) return false;
    return (bool)rdramMemory;
  }

  auto supports(const Emulator& emu) const -> bool override {
    return supportsAnyName(emu, {"Nintendo 64", "Nintendo 64DD"});
  }

  auto debugMemoryProfile() const -> RA::Debug::MemoryProfile override {
    return RA::Debug::MemoryProfile::N64;
  }

  auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 override {
    if(!buffer || size == 0) return 0;
    if(!ensureRdramMemory()) return 0;
    // rcheevos N64 map:
    // 0x000000-0x7fffff -> RDRAM
    auto rdramSize = rdramMemory->size();
    if(address >= rdramSize) return 0;
    auto readable = size;
    if(address + readable > rdramSize) readable = rdramSize - address;
    for(u32 index : range(readable)) buffer[index] = rdramMemory->read((address + index) ^ 3);
    return readable;
  }
};
#endif

#if defined(CORE_SFC)
struct SuperFamicomAdapter final : FixedConsoleAdapter<RC_CONSOLE_SUPER_NINTENDO> {
  mutable ares::Node::Object rootNode;
  mutable ares::Node::Debugger::Memory cpuWramMemory;
  mutable ares::Node::Debugger::Memory cartridgeRamMemory;
  mutable ares::Node::Debugger::Memory sa1BwramMemory;
  mutable ares::Node::Debugger::Memory sa1IramMemory;

  auto refreshMemoryNodes() const -> void {
    refreshMemoryNodeCache(rootNode, emulator ? &emulator->root : nullptr, {
      {"CPU WRAM", &cpuWramMemory},
      {"Cartridge RAM", &cartridgeRamMemory},
      {"SA1 BWRAM", &sa1BwramMemory},
      {"SA1 IRAM", &sa1IramMemory},
    });
  }

  auto supports(const Emulator& emu) const -> bool override {
    return supportsAnyName(emu, {"Super Famicom"});
  }

  auto romData(const Emulator& emu, std::vector<u8>& out) const -> bool override {
    out.clear();
    if(emulator && emulator->root) {
      std::vector<u8> slotA;
      std::vector<u8> slotB;

      auto readProgramFromPak = [&](const std::shared_ptr<vfs::directory>& pak, std::vector<u8>& target) -> bool {
        if(!pak) return false;
        auto fp = pak->read("program.rom");
        if(!fp) return false;
        target.resize(fp->size());
        for(u32 index : range(target.size())) target[index] = fp->read();
        return !target.empty();
      };

      for(auto node : ares::Node::enumerate<ares::Node::Peripheral>(emulator->root)) {
        if(!node || node->name() != "Sufami Turbo Cartridge") continue;
        auto parent = node->parent().lock();
        if(!parent) continue;
        auto pak = emulator->pak(node);
        if(!pak) continue;

        if(parent->name() == "Sufami Turbo Slot A") {
          if(slotA.empty()) readProgramFromPak(pak, slotA);
        } else if(parent->name() == "Sufami Turbo Slot B") {
          if(slotB.empty()) readProgramFromPak(pak, slotB);
        }
      }

      if(!slotA.empty()) {
        out = std::move(slotA);
        return true;
      }
      if(!slotB.empty()) {
        out = std::move(slotB);
        return true;
      }
    }

    return RA::readProgramRom(emu, out);
  }

  auto debugMemoryProfile() const -> RA::Debug::MemoryProfile override {
    return RA::Debug::MemoryProfile::SuperFamicom;
  }

  auto readMemory(u32 address, u8* buffer, u32 size) const -> u32 override {
    refreshMemoryNodes();
    // rcheevos SNES map:
    // 0x000000-0x01ffff: System RAM (128KB)
    // 0x020000-0x09ffff: Cartridge RAM / SA1 BWRAM
    // 0x0a0000-0x0a07ff: SA1 IRAM
    return Adapter::readRegions({
      {0x000000, 0x01ffff, ReadMode::Linear, 0x20000, &cpuWramMemory},
      {0x020000, 0x09ffff, ReadMode::Linear, 0x80000, &cartridgeRamMemory, &sa1BwramMemory},
      {0x0a0000, 0x0a07ff, ReadMode::Linear, 0x800, &sa1IramMemory},
    }, address, buffer, size);
  }
};
#endif

}

namespace RA::Platform::Detail {

auto registerNintendoAdapters(std::vector<Adapter*>& list) -> void {
#if defined(CORE_GB)
  static GameBoyAdapter gameBoy;
  list.push_back(&gameBoy);
#endif
#if defined(CORE_FC)
  static FamicomAdapter famicom;
  list.push_back(&famicom);
#endif
#if defined(CORE_GBA)
  static GameBoyAdvanceAdapter gameBoyAdvance;
  list.push_back(&gameBoyAdvance);
#endif
#if defined(CORE_N64)
  static Nintendo64Adapter nintendo64;
  list.push_back(&nintendo64);
#endif
#if defined(CORE_SFC)
  static SuperFamicomAdapter superFamicom;
  list.push_back(&superFamicom);
#endif
}

}

#endif
