#pragma once

#include "../retroachievements.hpp"

namespace RA::Platform::Hash {

#if ARES_ENABLE_RCHEEVOS
auto configureChdHashCallbacks(rc_hash_callbacks_t& callbacks) -> void;
#endif

}
