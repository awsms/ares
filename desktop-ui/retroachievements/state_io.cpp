#include "../desktop-ui.hpp"
#include "retroachievements.hpp"

#if ARES_ENABLE_RCHEEVOS

#include <cstring>

namespace {

auto saveStateSidecarPath(u32 slot) -> string {
  if(!emulator || !emulator->game) return {};
  auto statePath = emulator->locate(emulator->game->location, {".bs", slot}, settings.paths.saves);
  return {statePath, ".ra"};
}

auto clearProgressState(rc_client_t* client) -> void {
  if(!client) return;
  rc_client_deserialize_progress_sized(client, nullptr, 0);
}

auto clearProgressFor(RA::State& s) -> void {
  clearProgressState(s.client);
}

auto applyStateLoadSidecar(RA::State& s, u32 slot) -> void {
  if(!RA::hasClient(&s)) return;
  if(!emulator || !emulator->game) {
    clearProgressFor(s);
    return;
  }

  auto path = saveStateSidecarPath(slot);
  auto data = file::read(path);
  if(data.size() < 8) {
    clearProgressFor(s);
    return;
  }
  if(data[0] != 'R' || data[1] != 'C' || data[2] != 'H' || data[3] != 'V') {
    clearProgressFor(s);
    return;
  }

  u32 serializedSize = 0;
  std::memcpy(&serializedSize, &data[4], sizeof(serializedSize));
  if(data.size() < 8 + serializedSize) {
    clearProgressFor(s);
    return;
  }

  if(rc_client_deserialize_progress_sized(s.client, &data[8], serializedSize) != RC_OK) {
    clearProgressFor(s);
  }
}

}

namespace RA::StateIO {

auto onStateSaved(State& s, u32 slot) -> void {
  if(!ready(&s)) return;

  auto size = rc_client_progress_size(s.client);
  if(!size) return;

  std::vector<u8> data;
  data.resize(8 + size);
  data[0] = 'R';
  data[1] = 'C';
  data[2] = 'H';
  data[3] = 'V';
  u32 serializedSize = (u32)size;
  std::memcpy(&data[4], &serializedSize, sizeof(serializedSize));
  if(rc_client_serialize_progress_sized(s.client, &data[8], size) != RC_OK) return;

  auto path = saveStateSidecarPath(slot);
  if(path) file::write(path, {data.data(), (u32)data.size()});
}

auto onStateLoaded(State& s, u32 slot) -> void {
  if(!authed(&s)) return;
  if(s.session.hardcore) return;
  if(!s.session.ready) {
    s.session.pendingStateLoadSlot = (int)slot;
    return;
  }
  applyStateLoadSidecar(s, slot);
}

auto applyPendingAfterGameLoad(State& s) -> void {
  if(s.session.pendingStateLoadSlot < 0) return;
  applyStateLoadSidecar(s, (u32)s.session.pendingStateLoadSlot);
  s.session.pendingStateLoadSlot = -1;
}

auto clear(State& s) -> void {
  s.session.pendingStateLoadSlot = -1;
}

}

#endif
