#include "adapter.hpp"
#include "hash_chd.hpp"

#if ARES_ENABLE_RCHEEVOS

namespace RA::Platform {

auto available() -> bool {
  return !Detail::adapters().empty();
}

auto supports(const Emulator& emu) -> bool {
  return Detail::selectAdapter(emu) != nullptr;
}

auto consoleId(const Emulator& emu) -> u32 {
  if(auto* adapter = Detail::selectAdapter(emu)) return adapter->consoleId(emu);
  return RC_CONSOLE_UNKNOWN;
}

auto debugMemoryProfile(const Emulator& emu) -> Debug::MemoryProfile {
  if(auto* adapter = Detail::selectAdapter(emu)) return adapter->debugMemoryProfile();
  return Debug::MemoryProfile::Default;
}

auto filePath(const Emulator& emu) -> string {
  if(auto* adapter = Detail::selectAdapter(emu)) return adapter->filePath(emu);
  return {};
}

auto romData(const Emulator& emu, std::vector<u8>& out) -> bool {
  out.clear();
  if(auto* adapter = Detail::selectAdapter(emu)) return adapter->romData(emu, out);
  return false;
}

auto readMemory(u32 address, u8* buffer, u32 size) -> u32 {
  if(!emulator) return 0;
  if(auto* adapter = Detail::selectAdapter(*emulator)) return adapter->readMemory(address, buffer, size);
  return 0;
}

auto configureHashCallbacks(rc_hash_callbacks_t& callbacks) -> void {
  Hash::configureChdHashCallbacks(callbacks);
}

}

#endif
