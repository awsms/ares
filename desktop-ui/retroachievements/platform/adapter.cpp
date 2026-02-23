#include "adapter.hpp"

#if ARES_ENABLE_RCHEEVOS

namespace RA::Platform::Detail {

auto Adapter::supportsAnyName(const Emulator& emu, std::initializer_list<const char*> names) -> bool {
  for(auto name : names) {
    if(emu.name == name) return true;
  }
  return false;
}

auto Adapter::refreshMemoryNodeCache(
  ares::Node::Object& rootNode,
  const ares::Node::System* currentRoot,
  std::initializer_list<MemoryNodeBinding> bindings
) -> bool {
  if(!currentRoot || !*currentRoot) return false;
  ares::Node::Object effectiveRoot = *currentRoot;
  if(!effectiveRoot) return false;

  if(rootNode != effectiveRoot) {
    rootNode = effectiveRoot;
    for(auto binding : bindings) {
      if(binding.slot) binding.slot->reset();
    }
  }

  bool complete = true;
  for(auto binding : bindings) {
    if(binding.slot && !*binding.slot) {
      complete = false;
      break;
    }
  }
  if(complete) return true;

  for(auto memory : ares::Node::enumerate<ares::Node::Debugger::Memory>(effectiveRoot)) {
    if(!memory) continue;
    for(auto binding : bindings) {
      if(!binding.slot || *binding.slot) continue;
      if(memory->name() == binding.name) *binding.slot = memory;
    }
  }

  return true;
}

auto Adapter::readFromMemoryMode(
  const ares::Node::Debugger::Memory& memory, u32 offset, u8* buffer, u32 size, u32 maxSpan, ReadMode mode
) -> u32 {
  if(!memory || !buffer || size == 0) return 0;
  auto memorySize = memory->size();
  if(memorySize == 0) return 0;
  auto span = maxSpan ? min(memorySize, maxSpan) : memorySize;
  if(span == 0) return 0;

  if(mode == ReadMode::Linear || mode == ReadMode::LinearSwapped) {
    if(offset >= span) return 0;
    auto readable = size;
    if(offset + readable > span) readable = span - offset;
    for(u32 index : range(readable)) {
      auto indexOffset = offset + index;
      if(mode == ReadMode::LinearSwapped) indexOffset ^= 1;
      buffer[index] = memory->read(indexOffset);
    }
    return readable;
  }

  for(u32 index : range(size)) {
    auto indexOffset = (offset + index) % span;
    if(mode == ReadMode::WrappedSwapped) indexOffset ^= 1;
    buffer[index] = memory->read(indexOffset);
  }
  return size;
}

auto Adapter::readFromMemory(const ares::Node::Debugger::Memory& memory, u32 offset, u8* buffer, u32 size, u32 maxSpan) -> u32 {
  return readFromMemoryMode(memory, offset, buffer, size, maxSpan, ReadMode::Linear);
}

auto Adapter::readFromMemorySwapped(const ares::Node::Debugger::Memory& memory, u32 offset, u8* buffer, u32 size, u32 maxSpan) -> u32 {
  return readFromMemoryMode(memory, offset, buffer, size, maxSpan, ReadMode::LinearSwapped);
}

auto Adapter::readFromMemoryWrapped(const ares::Node::Debugger::Memory& memory, u32 offset, u8* buffer, u32 size) -> u32 {
  return readFromMemoryMode(memory, offset, buffer, size, 0, ReadMode::Wrapped);
}

auto Adapter::readFromMemoryWrappedSwapped(const ares::Node::Debugger::Memory& memory, u32 offset, u8* buffer, u32 size) -> u32 {
  return readFromMemoryMode(memory, offset, buffer, size, 0, ReadMode::WrappedSwapped);
}

auto Adapter::readRegion(const Region& region, u32 address, u8* buffer, u32 size) -> u32 {
  if(address < region.start || address > region.end) return 0;

  auto offset = address - region.start;
  auto readFrom = [&](const ares::Node::Debugger::Memory& memory) -> u32 {
    switch(region.mode) {
    case ReadMode::Linear:
      return readFromMemory(memory, offset, buffer, size, region.span);
    case ReadMode::LinearSwapped:
      return readFromMemorySwapped(memory, offset, buffer, size, region.span);
    case ReadMode::Wrapped:
      return readFromMemoryWrapped(memory, offset, buffer, size);
    case ReadMode::WrappedSwapped:
      return readFromMemoryWrappedSwapped(memory, offset, buffer, size);
    }
    return 0;
  };

  if(region.primary) {
    if(auto read = readFrom(*region.primary)) return read;
  }
  if(region.fallback) return readFrom(*region.fallback);
  return 0;
}

auto Adapter::readRegions(std::initializer_list<Region> regions, u32 address, u8* buffer, u32 size) -> u32 {
  if(!buffer || size == 0) return 0;
  for(auto& region : regions) {
    if(auto read = readRegion(region, address, buffer, size)) return read;
  }
  return 0;
}

}

#endif
