#include "adapter.hpp"

#if ARES_ENABLE_RCHEEVOS

namespace RA::Platform::Detail {

auto adapters() -> std::vector<Adapter*>& {
  static std::vector<Adapter*> list = [] {
    std::vector<Adapter*> out;
    registerNintendoAdapters(out);
    registerSegaAdapters(out);
    registerSonyAdapters(out);
    registerNecAdapters(out);
    registerSnkAdapters(out);
    registerMiscAdapters(out);
    return out;
  }();
  return list;
}

auto selectAdapter(const Emulator& emu) -> Adapter* {
  for(auto* adapter : adapters()) {
    if(adapter && adapter->supports(emu)) return adapter;
  }
  return nullptr;
}

}

#endif
