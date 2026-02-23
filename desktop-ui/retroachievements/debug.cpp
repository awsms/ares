#include "../desktop-ui.hpp"
#include "retroachievements.hpp"

#if ARES_ENABLE_RCHEEVOS

namespace RA::Debug {

auto print(const string& text) -> void {
  ::print("[RA-DEBUG] ", text, "\n");
}

auto logging() -> bool {
  return settings.achievements.debugLogging;
}

auto progress() -> bool {
  return settings.achievements.debugLogging && settings.achievements.debugProgress;
}

auto memory() -> bool {
  return settings.achievements.debugLogging && settings.achievements.debugMemory;
}

auto onGameLoaded(State& s, const Emulator& emulator, u32 consoleId) -> void {
  s.debug.memory.profile = Platform::debugMemoryProfile(emulator);
  s.debug.memory.resetCounters();
  if(!logging()) return;
  print({"onGameLoaded emulator=", emulator.name, " consoleId=", consoleId});
}

auto trackAddressByProfile(MemoryStats& d, u32 address) -> void {
  switch(d.profile) {
  case MemoryProfile::N64:
    if(address <= 0x007f'ffff) d.regionN64Rdram++;
    else d.regionOther++;
    return;
  case MemoryProfile::GameBoyAdvance:
    if(address <= 0x007fff) d.regionGbaIwram++;
    else if(address <= 0x047fff) d.regionGbaEwram++;
    else if(address <= 0x057fff) d.regionGbaSave++;
    else d.regionOther++;
    return;
  case MemoryProfile::Famicom:
    if(address <= 0x1fff) d.regionNesRam++;
    else d.regionOther++;
    return;
  case MemoryProfile::Sega8BitSG1000:
    if(address <= 0x5fff) d.regionSega8Ram++;
    else d.regionOther++;
    return;
  case MemoryProfile::Sega8Bit:
    if(address <= 0x1fff) d.regionSega8Ram++;
    else if(address <= 0x9fff) d.regionSega8CartRam++;
    else d.regionOther++;
    return;
  case MemoryProfile::MegaDrive:
    if(address <= 0x00ffff) d.regionMdRam++;
    else if(address <= 0x01ffff) d.regionMdCartRam++;
    else d.regionOther++;
    return;
  case MemoryProfile::MegaCd:
    if(address <= 0x00ffff) d.regionMdRam++;
    else if(address <= 0x08ffff) d.regionMdCdPrgRam++;
    else if(address <= 0x0affff) d.regionMdCdWordRam++;
    else d.regionOther++;
    return;
  case MemoryProfile::Mega32X:
    if(address <= 0x00ffff) d.regionMdRam++;
    else if(address <= 0x04ffff) d.regionMd32xRam++;
    else if(address <= 0x05ffff) d.regionMdCartRam++;
    else d.regionOther++;
    return;
  case MemoryProfile::NeoGeo:
    if(address <= 0x00ffff) d.regionNgWorkRam++;
    else if(address <= 0x01ffff) d.regionNgBackupRam++;
    else d.regionOther++;
    return;
  case MemoryProfile::NeoGeoPocket:
    if(address <= 0x2fff) d.regionNgpWorkRam++;
    else if(address <= 0x3fff) d.regionNgpSoundRam++;
    else d.regionOther++;
    return;
  case MemoryProfile::WonderSwan:
    if(address <= 0x00ffff) d.regionWsRam++;
    else if(address <= 0x08ffff) d.regionWsSave++;
    else d.regionOther++;
    return;
  case MemoryProfile::SuperFamicom:
    if(address <= 0x01ffff) d.regionSnesWram++;
    else if(address <= 0x09ffff) d.regionSnesSave++;
    else if(address <= 0x0a07ff) d.regionSnesSa1Iram++;
    else d.regionOther++;
    return;
  case MemoryProfile::PCEngine:
    if(address <= 0x001fff) d.regionPceRam++;
    else if(address <= 0x011fff) d.regionPceCdRam++;
    else if(address <= 0x041fff) d.regionPceSuperRam++;
    else if(address <= 0x0427ff) d.regionPceCdBram++;
    else d.regionOther++;
    return;
  case MemoryProfile::GameBoyFamily:
    if(address >= 0x00d000 && address <= 0x00dfff) {
      d.regionWram1++;
      return;
    }
    if(address <= 0x00ffff) {
      d.regionSystem++;
      return;
    }
    if(address >= 0x010000 && address <= 0x015fff) {
      d.regionWram2to7++;
      return;
    }
    if(address >= 0x016000 && address <= 0x033fff) {
      d.regionGbCartRam++;
      return;
    }
    d.regionOther++;
    return;
  case MemoryProfile::PlayStation:
    if(address <= 0x1fffff) d.regionPs1Ram++;
    else if(address >= 0x200000 && address <= 0x2003ff) d.regionPs1Scratch++;
    else d.regionOther++;
    return;
  case MemoryProfile::ColecoVision:
    if(address <= 0x03ff) d.regionCvRam++;
    else if(address >= 0x0400 && address <= 0x23ff) d.regionCvSgmLow++;
    else if(address >= 0x2400 && address <= 0x83ff) d.regionCvSgmHigh++;
    else d.regionOther++;
    return;
  case MemoryProfile::Default:
    d.regionOther++;
    return;
  }
}

auto trackMemoryRead(State& s, u32 address, u32 requested, u32 actual) -> void {
  if(!memory() || actual == 0) return;
  auto& d = s.debug.memory;
  d.readCalls++;
  d.readBytes += actual;
  d.lastAddress = address;
  d.lastSize = requested;
  d.lastRead = actual;
  if(actual == 0) d.zeroReads++;
  if(actual < requested) d.shortReads++;
  for(u32 index : range(actual)) {
    trackAddressByProfile(d, address + index);
  }
}

auto dumpPerSecondMemorySnapshot(State& s) -> void {
  if(!memory()) return;
  auto now = chrono::timestamp();
  auto& d = s.debug.memory;
  if(now == d.lastDumpSecond) return;
  d.lastDumpSecond = now;

  string line{"Per-second memory snapshot mem.reads=", d.readCalls, " bytes=", d.readBytes};
  switch(d.profile) {
  case MemoryProfile::N64:
    line.append(" n64rdram=", d.regionN64Rdram);
    break;
  case MemoryProfile::GameBoyAdvance:
    line.append(" iwram=", d.regionGbaIwram);
    line.append(" ewram=", d.regionGbaEwram);
    line.append(" saveram=", d.regionGbaSave);
    break;
  case MemoryProfile::Famicom:
    line.append(" ram=", d.regionNesRam);
    break;
  case MemoryProfile::Sega8BitSG1000:
  case MemoryProfile::Sega8Bit:
    line.append(" ram=", d.regionSega8Ram);
    if(d.regionSega8CartRam) line.append(" cartram=", d.regionSega8CartRam);
    break;
  case MemoryProfile::MegaDrive:
    line.append(" mdram=", d.regionMdRam);
    if(d.regionMdCartRam) line.append(" cartram=", d.regionMdCartRam);
    break;
  case MemoryProfile::MegaCd:
    line.append(" mdram=", d.regionMdRam);
    if(d.regionMdCdPrgRam) line.append(" cdprgram=", d.regionMdCdPrgRam);
    if(d.regionMdCdWordRam) line.append(" cdwordram=", d.regionMdCdWordRam);
    break;
  case MemoryProfile::Mega32X:
    line.append(" mdram=", d.regionMdRam);
    if(d.regionMd32xRam) line.append(" m32xram=", d.regionMd32xRam);
    if(d.regionMdCartRam) line.append(" cartram=", d.regionMdCartRam);
    break;
  case MemoryProfile::NeoGeo:
    line.append(" ngworkram=", d.regionNgWorkRam);
    if(d.regionNgBackupRam) line.append(" ngbackupram=", d.regionNgBackupRam);
    break;
  case MemoryProfile::NeoGeoPocket:
    line.append(" ngpworkram=", d.regionNgpWorkRam);
    if(d.regionNgpSoundRam) line.append(" ngpsoundram=", d.regionNgpSoundRam);
    break;
  case MemoryProfile::WonderSwan:
    line.append(" wsram=", d.regionWsRam);
    if(d.regionWsSave) line.append(" wssave=", d.regionWsSave);
    break;
  case MemoryProfile::SuperFamicom:
    line.append(" wram=", d.regionSnesWram);
    if(d.regionSnesSave) line.append(" cartram=", d.regionSnesSave);
    if(d.regionSnesSa1Iram) line.append(" sa1iram=", d.regionSnesSa1Iram);
    break;
  case MemoryProfile::PCEngine:
    line.append(" ram=", d.regionPceRam);
    if(d.regionPceCdRam) line.append(" cdram=", d.regionPceCdRam);
    if(d.regionPceSuperRam) line.append(" superram=", d.regionPceSuperRam);
    if(d.regionPceCdBram) line.append(" bram=", d.regionPceCdBram);
    break;
  case MemoryProfile::GameBoyFamily:
    line.append(" sys=", d.regionSystem);
    if(d.regionWram1) line.append(" wram1=", d.regionWram1);
    if(d.regionWram2to7) line.append(" wram2to7=", d.regionWram2to7);
    if(d.regionGbCartRam) line.append(" cartram=", d.regionGbCartRam);
    break;
  case MemoryProfile::PlayStation:
    line.append(" ps1ram=", d.regionPs1Ram);
    if(d.regionPs1Scratch) line.append(" scratch=", d.regionPs1Scratch);
    break;
  case MemoryProfile::ColecoVision:
    line.append(" ram=", d.regionCvRam);
    if(d.regionCvSgmLow) line.append(" sgmlow=", d.regionCvSgmLow);
    if(d.regionCvSgmHigh) line.append(" sgmhigh=", d.regionCvSgmHigh);
    break;
  case MemoryProfile::Default:
    if(d.regionOther) line.append(" other=", d.regionOther);
    break;
  }
  if(d.shortReads) line.append(" short=", d.shortReads);
  if(d.zeroReads) line.append(" zero=", d.zeroReads);
  line.append(" last=[0x", hex(d.lastAddress), " size=", d.lastSize, " read=", d.lastRead, "]");
  print(line, "\n");
  d.resetCounters();
}

}

#endif
