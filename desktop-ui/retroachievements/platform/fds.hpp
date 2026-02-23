#pragma once

#include "../retroachievements.hpp"

#if ARES_ENABLE_RCHEEVOS

namespace RA::Platform::FDS {

// Reads FDS media into the canonical byte layout rcheevos hashes:
// one 65500-byte payload per side, concatenated in side order.
// This accepts either ares' loaded pak side data or original .fds bytes.
auto readCanonicalRomData(const Emulator& emu, std::vector<u8>& out) -> bool;

}

#endif
