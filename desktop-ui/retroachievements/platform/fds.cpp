#include "../../desktop-ui.hpp"
#include "../retroachievements.hpp"
#include "fds.hpp"

#if ARES_ENABLE_RCHEEVOS

namespace {

constexpr u32 FdsCanonicalSideSize = 65500;
constexpr u32 FdsHeaderSize = 16;
constexpr u32 FdsLoadedSideMinimumSize = 0x12000;

auto readPakFile(const Emulator& emu, const char* name, std::vector<u8>& bytes) -> bool {
  if(!emu.game || !emu.game->pak) return false;
  auto fp = emu.game->pak->read(name);
  if(!fp) return false;

  bytes.resize(fp->size());
  for(u32 index : range(fp->size())) bytes[index] = fp->read();
  return true;
}

auto rebuildCanonicalFdsSide(const std::vector<u8>& loadedSide, std::vector<u8>& canonicalSide) -> bool {
  if(loadedSide.size() == FdsCanonicalSideSize) {
    canonicalSide = loadedSide;
    return true;
  }
  if(loadedSide.size() < FdsLoadedSideMinimumSize) return false;

  // ares stores loaded FDS sides in a drive-stream format with synthetic gaps,
  // sync markers (0x80), and CRC bytes between blocks. rcheevos hashes the
  // original canonical side payload instead: 65500 bytes of concatenated block
  // records with no transport padding. Rebuild that payload by skipping gaps,
  // validating sync bytes, and copying the meaningful block bytes back out.
  auto at = size_t{0};
  canonicalSide.clear();
  canonicalSide.reserve(FdsCanonicalSideSize);

  auto skip = [&](size_t count) -> bool {
    if(at + count > loadedSide.size()) return false;
    at += count;
    return true;
  };
  auto expect = [&](u8 value) -> bool {
    if(at >= loadedSide.size() || loadedSide[at] != value) return false;
    at++;
    return true;
  };
  auto appendBytes = [&](size_t count) -> bool {
    if(at + count > loadedSide.size()) return false;
    canonicalSide.insert(canonicalSide.end(), loadedSide.begin() + at, loadedSide.begin() + at + count);
    at += count;
    return true;
  };

  // Disk info block: leading boot gap, sync, 0x38-byte payload, 2-byte CRC.
  if(!skip(0x0e00) || !expect(0x80) || !appendBytes(0x38) || !skip(2)) return false;
  // File count block: inter-block gap, sync, 0x02-byte payload, 2-byte CRC.
  if(!skip(0x80) || !expect(0x80) || !appendBytes(0x02) || !skip(2)) return false;

  while(canonicalSide.size() < FdsCanonicalSideSize) {
    auto checkpoint = at;
    if(!skip(0x80)) break;
    if(at >= loadedSide.size() || loadedSide[at] != 0x80) {
      at = checkpoint;
      break;
    }
    at++;

    // File header block is 0x10 bytes and encodes the following file-data size
    // in bytes 0x0d-0x0e. The paired file-data block is block ID + payload.
    if(!appendBytes(0x10)) return false;
    auto sizeIndex = canonicalSide.size() - 0x10;
    u16 fileSize = canonicalSide[sizeIndex + 0x0d] | (canonicalSide[sizeIndex + 0x0e] << 8);

    if(!skip(2)) return false;
    if(!skip(0x80) || !expect(0x80)) return false;
    if(!appendBytes(1 + fileSize)) return false;
    if(!skip(2)) return false;
  }

  if(canonicalSide.size() > FdsCanonicalSideSize) canonicalSide.resize(FdsCanonicalSideSize);
  else if(canonicalSide.size() < FdsCanonicalSideSize) canonicalSide.resize(FdsCanonicalSideSize, 0x00);
  return true;
}

auto normalizeCanonicalFdsImage(std::vector<u8> bytes, std::vector<u8>& out) -> bool {
  auto offset = 0u;
  if(bytes.size() % FdsCanonicalSideSize == FdsHeaderSize) offset = FdsHeaderSize;
  if(bytes.size() < offset + FdsCanonicalSideSize) return false;

  out.assign(bytes.begin() + offset, bytes.end());
  return true;
}

auto appendCanonicalPakSide(const Emulator& emu, const char* name, std::vector<u8>& out) -> bool {
  std::vector<u8> loadedSide;
  if(!readPakFile(emu, name, loadedSide)) return false;

  std::vector<u8> canonicalSide;
  if(!rebuildCanonicalFdsSide(loadedSide, canonicalSide)) return false;

  out.insert(out.end(), canonicalSide.begin(), canonicalSide.end());
  return true;
}

auto appendCanonicalPakSides(const Emulator& emu, std::vector<u8>& out) -> bool {
  auto appended = false;
  appended |= appendCanonicalPakSide(emu, "disk1.sideA", out);
  appended |= appendCanonicalPakSide(emu, "disk1.sideB", out);
  appended |= appendCanonicalPakSide(emu, "disk2.sideA", out);
  appended |= appendCanonicalPakSide(emu, "disk2.sideB", out);
  return appended;
}

auto readCanonicalFdsFromPath(const string& path, std::vector<u8>& out) -> bool {
  if(!path) return false;

  if(path.iendsWith(".zip")) {
    Decode::ZIP archive;
    if(!archive.open(path)) return false;

    for(auto& file : archive.file) {
      if(!file.name.iendsWith(".fds")) continue;
      if(normalizeCanonicalFdsImage(archive.extract(file), out)) return true;
    }

    if(!archive.file.empty()) {
      if(normalizeCanonicalFdsImage(archive.extract(archive.file.front()), out)) return true;
    }
    return false;
  }

  if(path.iendsWith(".fds")) {
    return normalizeCanonicalFdsImage(file::read(path), out);
  }

  return false;
}

}

namespace RA::Platform::FDS {

auto readCanonicalRomData(const Emulator& emu, std::vector<u8>& out) -> bool {
  out.clear();
  if(emu.name != "Famicom Disk System") return false;

  if(appendCanonicalPakSides(emu, out)) return true;
  return readCanonicalFdsFromPath(emu.game ? emu.game->location : string{}, out);
}

}

#endif
