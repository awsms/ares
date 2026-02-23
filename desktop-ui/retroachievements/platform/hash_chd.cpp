#include "../../desktop-ui.hpp"
#include "../retroachievements.hpp"
#include "hash_chd.hpp"

#if ARES_ENABLE_RCHEEVOS

namespace {

#if defined(ARES_ENABLE_CHD)
struct ChdTrackHandle {
  Decode::CHD chd;
  u32 firstSector = 0;
  u32 lastSector = 0;
  u32 dataOffset = 16;
  u32 dataSize = 2048;
  bool hasLastRead = false;
  u32 lastRequestedSector = 0;
  u32 lastMappedSector = 0;
};

struct CdHashTrackHandle {
  bool isChd = false;
  void* inner = nullptr;
  rc_hash_cdreader_t fallback{};
};

auto isAudioTrack(const Decode::CHD::Track& track) -> bool {
  return (bool)track.type.find("AUDIO");
}

auto findDataIndex(const Decode::CHD::Track& track) -> const Decode::CHD::Index* {
  for(auto& index : track.indices) {
    if(index.number == 1 && index.end >= index.lba) return &index;
  }
  for(auto& index : track.indices) {
    if(index.end >= index.lba) return &index;
  }
  return nullptr;
}

auto openChdTrack(const char* path, u32 requestedTrack) -> void* {
  if(!path) return nullptr;

  auto handle = std::make_unique<ChdTrackHandle>();
  if(!handle->chd.load(path)) return nullptr;
  if(handle->chd.tracks.empty()) return nullptr;

  const Decode::CHD::Track* selectedTrack = nullptr;
  if(requestedTrack == RC_HASH_CDTRACK_FIRST_DATA) {
    for(auto& track : handle->chd.tracks) {
      if(isAudioTrack(track)) continue;
      if(findDataIndex(track)) {
        selectedTrack = &track;
        break;
      }
    }
  } else if(requestedTrack == RC_HASH_CDTRACK_LAST) {
    for(auto index = (s32)handle->chd.tracks.size() - 1; index >= 0; --index) {
      auto& track = handle->chd.tracks[index];
      if(findDataIndex(track)) {
        selectedTrack = &track;
        break;
      }
    }
  } else if(requestedTrack == RC_HASH_CDTRACK_LARGEST) {
    u32 largest = 0;
    for(auto& track : handle->chd.tracks) {
      auto count = track.sectorCount();
      if(count > largest && findDataIndex(track)) {
        largest = count;
        selectedTrack = &track;
      }
    }
  } else {
    for(auto& track : handle->chd.tracks) {
      if(track.number != requestedTrack) continue;
      if(findDataIndex(track)) {
        selectedTrack = &track;
        break;
      }
    }
  }

  if(!selectedTrack) return nullptr;
  auto* dataIndex = findDataIndex(*selectedTrack);
  if(!dataIndex) return nullptr;

  // Keep first_track_sector aligned with rcheevos/default cdreader semantics:
  // data track first sector (index1), not pregap/index0.
  handle->firstSector = dataIndex->lba;
  handle->lastSector = dataIndex->end;
  handle->dataOffset = (bool)selectedTrack->type.find("MODE2") ? 24 : 16;
  return handle.release();
}

auto chdReadSector(void* trackHandle, u32 sector, void* buffer, size_t requestedBytes) -> size_t {
  auto* handle = (ChdTrackHandle*)trackHandle;
  if(!handle || !buffer || requestedBytes == 0) return 0;

  u32 mappedSector = sector;
  auto mappedAbsolute = sector;
  auto mappedRelative = sector + handle->firstSector;
  auto absoluteValid = mappedAbsolute >= handle->firstSector && mappedAbsolute <= handle->lastSector;
  auto relativeValid = mappedRelative >= handle->firstSector && mappedRelative <= handle->lastSector;

  bool relativeContinues = false;
  bool relativeRepeats = false;
  if(handle->hasLastRead) {
    relativeContinues = (sector == handle->lastRequestedSector + 1) && (mappedRelative == handle->lastMappedSector + 1);
    relativeRepeats = (sector == handle->lastRequestedSector) && (mappedRelative == handle->lastMappedSector);
  }

  // rcheevos probes the ISO PVD at first_track_sector + 16 using absolute
  // addressing. Pin that probe to absolute mode, but do not break an active
  // relative stream (large file reads can cross sector first+16).
  if(sector == handle->firstSector + 16 && absoluteValid && !(relativeContinues || relativeRepeats)) {
    mappedSector = mappedAbsolute;
  } else if(!absoluteValid && relativeValid) {
    mappedSector = mappedRelative;
  } else if(absoluteValid && !relativeValid) {
    mappedSector = mappedAbsolute;
  } else if(absoluteValid && relativeValid) {
    // Some rcheevos disc hashing paths mix absolute LBAs and ISO-relative sectors in one run.
    // Use continuity (requested+mapped both incrementing) to disambiguate overlap.
    if(handle->hasLastRead) {
      bool absoluteContinues = (sector == handle->lastRequestedSector + 1) && (mappedAbsolute == handle->lastMappedSector + 1);
      bool absoluteRepeats = (sector == handle->lastRequestedSector) && (mappedAbsolute == handle->lastMappedSector);

      if(relativeContinues || relativeRepeats) mappedSector = mappedRelative;
      else if(absoluteContinues || absoluteRepeats) mappedSector = mappedAbsolute;
      else mappedSector = mappedRelative;
    } else {
      mappedSector = mappedAbsolute;
    }
  }

  handle->hasLastRead = true;
  handle->lastRequestedSector = sector;
  handle->lastMappedSector = mappedSector;

  if(mappedSector < handle->firstSector || mappedSector > handle->lastSector) return 0;

  auto* out = (u8*)buffer;
  size_t total = 0;
  while(requestedBytes > 0 && mappedSector <= handle->lastSector) {
    auto sectorData = handle->chd.read(mappedSector);
    if(sectorData.empty()) break;
    const u8* source = sectorData.data();
    size_t available = sectorData.size();

    if(available > handle->dataSize) {
      u32 sectorOffset = handle->dataOffset;
      if(sectorOffset >= available) break;
      source += sectorOffset;
      available -= sectorOffset;
      available = min<size_t>(available, handle->dataSize);
    }

    auto chunk = min(requestedBytes, available);
    memory::copy(out, source, chunk);
    out += chunk;
    total += chunk;
    requestedBytes -= chunk;
    mappedSector++;
  }
  return total;
}

auto chdFirstTrackSector(void* trackHandle) -> u32 {
  auto* handle = (ChdTrackHandle*)trackHandle;
  if(!handle) return 0;
  return handle->firstSector;
}

auto chdCloseTrack(void* trackHandle) -> void {
  delete (ChdTrackHandle*)trackHandle;
}

auto hashOpenTrackIterator(const char* path, u32 track, const rc_hash_iterator_t* iterator) -> void* {
  auto handle = std::make_unique<CdHashTrackHandle>();
  if(path && string{path}.iendsWith(".chd")) {
    handle->isChd = true;
    handle->inner = openChdTrack(path, track);
    if(!handle->inner) return nullptr;
    return handle.release();
  }

  rc_hash_get_default_cdreader(&handle->fallback);
  if(!handle->fallback.open_track_iterator) return nullptr;
  handle->inner = handle->fallback.open_track_iterator(path, track, iterator);
  if(!handle->inner) return nullptr;
  return handle.release();
}

auto hashOpenTrack(const char* path, u32 track) -> void* {
  auto handle = std::make_unique<CdHashTrackHandle>();
  if(path && string{path}.iendsWith(".chd")) {
    handle->isChd = true;
    handle->inner = openChdTrack(path, track);
    if(!handle->inner) return nullptr;
    return handle.release();
  }

  rc_hash_get_default_cdreader(&handle->fallback);
  if(!handle->fallback.open_track) return nullptr;
  handle->inner = handle->fallback.open_track(path, track);
  if(!handle->inner) return nullptr;
  return handle.release();
}

auto hashReadSector(void* trackHandle, u32 sector, void* buffer, size_t requestedBytes) -> size_t {
  auto* handle = (CdHashTrackHandle*)trackHandle;
  if(!handle || !handle->inner) return 0;
  if(handle->isChd) return chdReadSector(handle->inner, sector, buffer, requestedBytes);
  if(handle->fallback.read_sector) return handle->fallback.read_sector(handle->inner, sector, buffer, requestedBytes);
  return 0;
}

auto hashFirstTrackSector(void* trackHandle) -> u32 {
  auto* handle = (CdHashTrackHandle*)trackHandle;
  if(!handle || !handle->inner) return 0;
  if(handle->isChd) return chdFirstTrackSector(handle->inner);
  if(handle->fallback.first_track_sector) return handle->fallback.first_track_sector(handle->inner);
  return 0;
}

auto hashCloseTrack(void* trackHandle) -> void {
  auto* handle = (CdHashTrackHandle*)trackHandle;
  if(!handle) return;
  if(handle->inner) {
    if(handle->isChd) chdCloseTrack(handle->inner);
    else if(handle->fallback.close_track) handle->fallback.close_track(handle->inner);
  }
  delete handle;
}
#endif

}

namespace RA::Platform::Hash {

auto configureChdHashCallbacks(rc_hash_callbacks_t& callbacks) -> void {
#if defined(ARES_ENABLE_CHD)
  callbacks.cdreader.open_track = hashOpenTrack;
  callbacks.cdreader.open_track_iterator = hashOpenTrackIterator;
  callbacks.cdreader.read_sector = hashReadSector;
  callbacks.cdreader.close_track = hashCloseTrack;
  callbacks.cdreader.first_track_sector = hashFirstTrackSector;
#else
  (void)callbacks;
#endif
}

}

#endif
