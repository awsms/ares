#pragma once

#include <memory>
#include <string>

#if ARES_ENABLE_RCHEEVOS
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

extern "C" {
#include <rcheevos/rcheevos/include/rc_api_request.h>
#include <rcheevos/rcheevos/include/rc_client.h>
#include <rcheevos/rcheevos/include/rc_hash.h>
}
#endif

struct Emulator;
struct RetroAchievements;

namespace RA {
struct State;
namespace Auth {
auto applyStartup(State& s, RetroAchievements& ra) -> void;
}
}

// Public API
struct RetroAchievements {
  RetroAchievements();
  ~RetroAchievements();

  auto initialize() -> void;
  auto shutdown() -> void;

  auto gameLoaded() -> void;
  auto gameUnloaded() -> void;
  auto reset() -> void;
  auto stateSaved(u32 slot) -> void;
  auto stateLoaded(u32 slot) -> void;
  auto frame() -> void;
  auto idle() -> void;
  auto pumpServerResponses() -> void;

  auto setEnabled(bool enabled) -> void;
  auto enabled() const -> bool;
  auto startupSetEncoreEnabled(bool enabled) -> void;
  auto startupLoginWithPassword(const string& username, const string& password) -> void;
  auto startupLogout() -> void;
  auto loginWithPassword(const string& username, const string& password) -> void;
  auto logout() -> void;
  auto hardcore() const -> bool;
  auto enableHardcore(bool enabled) -> void;
  auto confirmDisableHardcore(const string& reason) -> bool;
  auto isToolPanelBlockedInHardcore(const string& panelName) const -> bool;

  auto hasUser() const -> bool;
  auto username() const -> string;
  auto userScore() const -> u32;
  auto avatarUrl() const -> string;
  auto sessionReady() const -> bool;
  auto supported() const -> bool;

  // Internal: callback helpers in retroachievements.cpp use these.
  auto state() -> RA::State*;
  auto state() const -> const RA::State*;

private:
  struct Backend;
  struct RealBackend;
  struct NullBackend;
  static auto makeBackend() -> std::unique_ptr<Backend>;
  friend auto RA::Auth::applyStartup(RA::State& s, RetroAchievements& ra) -> void;

  std::unique_ptr<Backend> _backend;
  std::unique_ptr<RA::State> _state;
  bool _startupLoginRequested = false;
  bool _startupLogoutRequested = false;
  bool _startupEncoreEnabled = false;
  string _startupLoginUsername;
  string _startupLoginPassword;
};

extern RetroAchievements retroAchievements;

namespace RA {

// Internal implementation declarations and shared types
namespace Debug {

enum class MemoryProfile : u32 {
  Default,
  N64,
  GameBoyAdvance,
  Famicom,
  Sega8Bit,
  Sega8BitSG1000,
  MegaCd,
  Mega32X,
  NeoGeo,
  NeoGeoPocket,
  WonderSwan,
  MegaDrive,
  SuperFamicom,
  PCEngine,
  GameBoyFamily,
  PlayStation,
  ColecoVision,
};

struct MemoryStats {
  MemoryProfile profile = MemoryProfile::Default;
  u64 readCalls = 0;
  u64 readBytes = 0;
  u64 regionSystem = 0;   // 0x000000-0x00ffff (normal map)
  u64 regionWram1 = 0;    // 0x00d000-0x00dfff (forced WRAM1)
  u64 regionWram2to7 = 0; // 0x010000-0x015fff
  u64 regionGbCartRam = 0;  // 0x016000-0x033fff
  u64 regionN64Rdram = 0;   // 0x000000-0x7fffff
  u64 regionGbaIwram = 0;   // 0x000000-0x007fff
  u64 regionGbaEwram = 0;   // 0x008000-0x047fff
  u64 regionGbaSave = 0;    // 0x048000-0x057fff
  u64 regionNesRam = 0;     // 0x0000-0x1fff
  u64 regionSega8Ram = 0;   // SG/SMS/GG system RAM
  u64 regionSega8CartRam = 0;  // SG/SMS/GG cartridge RAM (virtual RA region)
  u64 regionMdRam = 0;      // Mega Drive main RAM
  u64 regionMd32xRam = 0;   // Mega 32X additional RAM
  u64 regionMdCartRam = 0;  // Mega Drive cartridge RAM (virtual RA region)
  u64 regionMdCdPrgRam = 0; // Mega CD PRG RAM
  u64 regionMdCdWordRam = 0; // Mega CD WORD RAM
  u64 regionNgWorkRam = 0;  // Neo Geo 68K/System Work RAM
  u64 regionNgBackupRam = 0; // Neo Geo MVS backup RAM
  u64 regionNgpWorkRam = 0; // Neo Geo Pocket work RAM
  u64 regionNgpSoundRam = 0; // Neo Geo Pocket sound RAM
  u64 regionWsRam = 0;      // WonderSwan system RAM
  u64 regionWsSave = 0;     // WonderSwan save RAM/EEPROM
  u64 regionSnesWram = 0;   // 0x000000-0x01ffff
  u64 regionSnesSave = 0;   // 0x020000-0x09ffff
  u64 regionSnesSa1Iram = 0; // 0x0a0000-0x0a07ff
  u64 regionPceRam = 0;     // 0x000000-0x001fff
  u64 regionPceCdRam = 0;   // 0x002000-0x011fff
  u64 regionPceSuperRam = 0; // 0x012000-0x041fff
  u64 regionPceCdBram = 0;  // 0x042000-0x0427ff
  u64 regionPs1Ram = 0;     // 0x000000-0x1fffff
  u64 regionPs1Scratch = 0; // 0x200000-0x2003ff
  u64 regionCvRam = 0;      // 0x000000-0x0003ff
  u64 regionCvSgmLow = 0;   // 0x000400-0x0023ff
  u64 regionCvSgmHigh = 0;  // 0x002400-0x0083ff
  u64 regionOther = 0;
  u64 shortReads = 0;
  u64 zeroReads = 0;
  u32 lastAddress = 0;
  u32 lastSize = 0;
  u32 lastRead = 0;
  u64 lastDumpSecond = 0;

  auto resetCounters() -> void {
    readCalls = 0;
    readBytes = 0;
    regionSystem = 0;
    regionWram1 = 0;
    regionWram2to7 = 0;
    regionGbCartRam = 0;
    regionN64Rdram = 0;
    regionGbaIwram = 0;
    regionGbaEwram = 0;
    regionGbaSave = 0;
    regionNesRam = 0;
    regionSega8Ram = 0;
    regionSega8CartRam = 0;
    regionMdRam = 0;
    regionMd32xRam = 0;
    regionMdCartRam = 0;
    regionMdCdPrgRam = 0;
    regionMdCdWordRam = 0;
    regionNgWorkRam = 0;
    regionNgBackupRam = 0;
    regionNgpWorkRam = 0;
    regionNgpSoundRam = 0;
    regionWsRam = 0;
    regionWsSave = 0;
    regionSnesWram = 0;
    regionSnesSave = 0;
    regionSnesSa1Iram = 0;
    regionPceRam = 0;
    regionPceCdRam = 0;
    regionPceSuperRam = 0;
    regionPceCdBram = 0;
    regionPs1Ram = 0;
    regionPs1Scratch = 0;
    regionCvRam = 0;
    regionCvSgmLow = 0;
    regionCvSgmHigh = 0;
    regionOther = 0;
    shortReads = 0;
    zeroReads = 0;
  }
};

auto print(const string& text) -> void;
auto logging() -> bool;
auto progress() -> bool;
auto memory() -> bool;

auto onGameLoaded(State& s, const Emulator& emulator, u32 consoleId) -> void;
auto trackMemoryRead(State& s, u32 address, u32 requested, u32 actual) -> void;
auto dumpPerSecondMemorySnapshot(State& s) -> void;

}

namespace Images {

auto start() -> void;
auto stop() -> void;
auto consumeDirty() -> bool;
#if ARES_ENABLE_RCHEEVOS
auto fetchAchievementBadge(const rc_client_achievement_t* achievement, bool unlocked, u32 size) -> image;
auto fetchUserAvatar(const string& username, const string& url, u32 size) -> image;
auto fetchGameImage(const rc_client_game_t* game, u32 size = 0) -> image;
auto prefetchUserAvatar(const rc_client_user_t* user) -> void;
auto prefetchGameImage(const rc_client_game_t* game) -> void;
#endif

}

#if ARES_ENABLE_RCHEEVOS

struct HttpRequest {
  u64 generation = 0;
  string url;
  string postData;
  rc_client_server_callback_t callback = nullptr;
  void* callbackData = nullptr;
};

struct HttpResponse {
  u64 generation = 0;
  rc_client_server_callback_t callback = nullptr;
  void* callbackData = nullptr;
  long statusCode = RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
  std::vector<char> body;
};

struct AuthState {
  bool hasUser = false;
  bool pendingLogin = false;
  rc_client_async_handle_t* loginHandle = nullptr;

  string username;
  string pendingUsername;
  string pendingToken;

  std::atomic<u64> generation = 1;
};

struct SessionState {
  bool ready = false;
  bool hardcore = false;
  bool gameLoaded = false;
  bool hasAchievements = false;
  bool hasLeaderboards = false;
  bool hasVisibleAchievements = false;
  bool hasVisibleLeaderboards = false;
  bool warnedUnsupportedBuild = false;
  bool encoreMode = false;
  int pendingStateLoadSlot = -1;
  rc_client_async_handle_t* loadHandle = nullptr;

  std::unordered_set<u32> initiallyUnlockedIds;
  std::vector<u8> pendingRomData;

  std::atomic<u64> loadGeneration = 1;
};

struct HttpState {
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<HttpRequest> requestQueue;
  std::deque<HttpResponse> responseQueue;
  std::thread thread;
  bool stopping = false;
  bool curlGlobalInitialized = false;

  std::atomic<u64> generation = 1;
};

struct DebugState {
  Debug::MemoryStats memory;
};

struct State {
  rc_client_t* client = nullptr;

  bool enabled = false;
  bool shuttingDown = false;

  AuthState auth;
  SessionState session;
  HttpState http;
  DebugState debug;
};

inline auto print(const string& text) -> void {
  print("[RA] ", text, "\n");
}

inline auto readProgramRom(const Emulator& emu, std::vector<u8>& out) -> bool {
  out.clear();
  if(!emu.game || !emu.game->pak) return false;
  if(auto fp = emu.game->pak->read("program.rom")) {
    out.resize(fp->size());
    for(u32 index : range(out.size())) out[index] = fp->read();
  }
  return !out.empty();
}

inline auto stripLineEndings(std::string& text) -> void {
  while(!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
}

inline auto hasClient(const State* s) -> bool {
  return s && s->client;
}

inline auto alive(const State* s) -> bool {
  return hasClient(s) && !s->shuttingDown;
}

inline auto active(const State* s) -> bool {
  return alive(s) && s->enabled;
}

inline auto authed(const State* s) -> bool {
  return active(s) && s->auth.hasUser;
}

inline auto ready(const State* s) -> bool {
  return authed(s) && s->session.ready;
}

inline auto clearSession(State& s) -> void {
  s.session.ready = false;
  s.session.hasAchievements = false;
  s.session.hasLeaderboards = false;
  s.session.hasVisibleAchievements = false;
  s.session.hasVisibleLeaderboards = false;
  s.session.warnedUnsupportedBuild = false;
  s.session.loadHandle = nullptr;
  s.session.initiallyUnlockedIds.clear();
  s.session.pendingRomData.clear();
  s.session.pendingStateLoadSlot = -1;
}

inline auto clearAuth(State& s) -> void {
  s.auth.hasUser = false;
  s.auth.pendingLogin = false;
  s.auth.loginHandle = nullptr;
  s.auth.username = "";
  s.auth.pendingUsername = "";
  s.auth.pendingToken = "";
  s.session.hardcore = false;
}

inline auto clearPendingLogin(State& s) -> void {
  s.auth.pendingLogin = false;
  s.auth.loginHandle = nullptr;
  s.auth.pendingUsername = "";
  s.auth.pendingToken = "";
}

inline auto abortLoginAsync(State& s) -> void {
  if(s.client && s.auth.loginHandle) rc_client_abort_async(s.client, s.auth.loginHandle);
  clearPendingLogin(s);
}

inline auto abortLoadAsync(State& s) -> void {
  if(s.client && s.session.loadHandle) rc_client_abort_async(s.client, s.session.loadHandle);
  s.session.loadHandle = nullptr;
}

inline auto invalidateAuth(State& s) -> void {
  s.auth.generation.fetch_add(1);
}

inline auto invalidate(State& s) -> void {
  s.session.loadGeneration.fetch_add(1);
  s.http.generation.fetch_add(1);
}

inline auto managerFromClient(const rc_client_t* client) -> RetroAchievements* {
  return client ? (RetroAchievements*)rc_client_get_userdata(client) : nullptr;
}

inline auto stateFromClient(const rc_client_t* client) -> State* {
  auto* manager = managerFromClient(client);
  return manager ? manager->state() : nullptr;
}

namespace Platform {
auto available() -> bool;
auto supports(const Emulator& emu) -> bool;
auto consoleId(const Emulator& emu) -> u32;
auto debugMemoryProfile(const Emulator& emu) -> Debug::MemoryProfile;
auto filePath(const Emulator& emu) -> string;
auto romData(const Emulator& emu, std::vector<u8>& out) -> bool;
auto readMemory(u32 address, u8* buffer, u32 size) -> u32;
auto configureHashCallbacks(rc_hash_callbacks_t& callbacks) -> void;
}

namespace Http {
auto start(State& s) -> void;
auto stop(State& s) -> void;
auto clearQueues(State& s) -> void;
auto pump(State& s) -> void;
auto serverCall(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callbackData, rc_client_t* client)
  -> void;
}

namespace Client {
auto create(State& s, RetroAchievements* owner) -> void;
auto destroy(State& s) -> void;
auto beginGameLoad(State& s) -> void;
auto teardownSession(State& s, bool gameStillLoaded, bool unloadClientGame = true) -> void;
auto unloadGame(State& s) -> void;
auto onFrame(State& s) -> void;
auto onIdle(State& s) -> void;
auto reset(State& s) -> void;
}

namespace Auth {
auto applyStartup(State& s, RetroAchievements& ra) -> void;
auto loginWithPassword(State& s, const string& user, const string& pass) -> void;
auto logout(State& s) -> void;
auto loadTokenLogin(State& s) -> void;
}

namespace StateIO {
auto onStateSaved(State& s, u32 slot) -> void;
auto onStateLoaded(State& s, u32 slot) -> void;
auto applyPendingAfterGameLoad(State& s) -> void;
auto clear(State& s) -> void;
}

#else

struct State {};

#endif

}
