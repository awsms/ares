#include "../desktop-ui.hpp"
#include "retroachievements.hpp"

struct RetroAchievements::Backend {
  virtual ~Backend() = default;

  virtual auto initialize(RetroAchievements& ra) -> void = 0;
  virtual auto shutdown(RetroAchievements& ra) -> void = 0;

  virtual auto gameLoaded(RetroAchievements& ra) -> void = 0;
  virtual auto gameUnloaded(RetroAchievements& ra) -> void = 0;
  virtual auto reset(RetroAchievements& ra) -> void = 0;
  virtual auto stateSaved(RetroAchievements& ra, u32 slot) -> void = 0;
  virtual auto stateLoaded(RetroAchievements& ra, u32 slot) -> void = 0;
  virtual auto frame(RetroAchievements& ra) -> void = 0;
  virtual auto idle(RetroAchievements& ra) -> void = 0;
  virtual auto pumpServerResponses(RetroAchievements& ra) -> void = 0;

  virtual auto setEnabled(RetroAchievements& ra, bool enabled) -> void = 0;
  virtual auto enabled(const RetroAchievements& ra) const -> bool = 0;
  virtual auto startupSetEncoreEnabled(RetroAchievements& ra, bool enabled) -> void = 0;
  virtual auto startupLoginWithPassword(RetroAchievements& ra, const string& username, const string& password) -> void = 0;
  virtual auto startupLogout(RetroAchievements& ra) -> void = 0;
  virtual auto loginWithPassword(RetroAchievements& ra, const string& username, const string& password) -> void = 0;
  virtual auto logout(RetroAchievements& ra) -> void = 0;
  virtual auto hardcore(const RetroAchievements& ra) const -> bool = 0;
  virtual auto enableHardcore(RetroAchievements& ra, bool enabled) -> void = 0;
  virtual auto confirmDisableHardcore(RetroAchievements& ra, const string& reason) -> bool = 0;
  virtual auto isToolPanelBlockedInHardcore(const RetroAchievements& ra, const string& panelName) const -> bool = 0;

  virtual auto hasUser(const RetroAchievements& ra) const -> bool = 0;
  virtual auto username(const RetroAchievements& ra) const -> string = 0;
  virtual auto userScore(const RetroAchievements& ra) const -> u32 = 0;
  virtual auto avatarUrl(const RetroAchievements& ra) const -> string = 0;
  virtual auto sessionReady(const RetroAchievements& ra) const -> bool = 0;
  virtual auto supported(const RetroAchievements& ra) const -> bool = 0;
};

#if ARES_ENABLE_RCHEEVOS

struct RetroAchievements::RealBackend final : RetroAchievements::Backend {
  auto initialize(RetroAchievements& ra) -> void override {
    if(ra.state()) return;
    ra._state = std::make_unique<RA::State>();
    auto* s = ra.state();
    if(!s) return;

    s->enabled = settings.general.retroAchievements;

    RA::Client::create(*s, &ra);
    if(!s->client) {
      RA::print("Failed to create rc_client");
      ra._state.reset();
      return;
    }

    s->session.encoreMode = ra._startupEncoreEnabled;
    rc_client_set_encore_mode_enabled(s->client, s->session.encoreMode ? 1 : 0);
    s->session.hardcore = settings.achievements.hardcore;
    rc_client_set_hardcore_enabled(s->client, s->session.hardcore ? 1 : 0);
    if(s->session.hardcore && settings.debugServer.enabled) {
      settings.debugServer.enabled = false;
      nall::GDB::server.close();
    }

    RA::Http::start(*s);
    if(!s->http.curlGlobalInitialized) {
      RA::Client::destroy(*s);
      ra._state.reset();
      return;
    }
    RA::Images::start();

    if(RA::Debug::logging()) {
      RA::print({"RetroAchievements initialized (enabled=", s->enabled ? "yes" : "no", ")"});
    }
    RA::Auth::applyStartup(*s, ra);
  }

  auto shutdown(RetroAchievements& ra) -> void override {
    auto* s = ra.state();
    if(!s) return;

    s->shuttingDown = true;
    RA::invalidateAuth(*s);
    RA::abortLoginAsync(*s);
    RA::Client::teardownSession(*s, s->session.gameLoaded);
    RA::Images::stop();
    RA::Http::stop(*s);

    if(s->client) RA::Client::destroy(*s);

    ra._state.reset();
  }

  auto gameLoaded(RetroAchievements& ra) -> void override {
    auto* s = ra.state();
    if(!RA::alive(s)) return;
    s->session.gameLoaded = true;
    s->session.warnedUnsupportedBuild = false;
    s->session.initiallyUnlockedIds.clear();
    RA::Client::beginGameLoad(*s);
  }

  auto gameUnloaded(RetroAchievements& ra) -> void override {
    auto* s = ra.state();
    if(!RA::alive(s)) return;
    RA::Client::unloadGame(*s);
  }

  auto reset(RetroAchievements& ra) -> void override {
    auto* s = ra.state();
    if(!RA::alive(s)) return;
    RA::Client::reset(*s);
  }

  auto stateSaved(RetroAchievements& ra, u32 slot) -> void override {
    auto* s = ra.state();
    if(!s || s->shuttingDown) return;
    RA::StateIO::onStateSaved(*s, slot);
  }

  auto stateLoaded(RetroAchievements& ra, u32 slot) -> void override {
    auto* s = ra.state();
    if(!s || s->shuttingDown) return;
    if(s->session.hardcore) return;
    RA::StateIO::onStateLoaded(*s, slot);
  }

  auto frame(RetroAchievements& ra) -> void override {
    auto* s = ra.state();
    if(!s || s->shuttingDown) return;
    RA::Client::onFrame(*s);
  }

  auto idle(RetroAchievements& ra) -> void override {
    auto* s = ra.state();
    if(!s || s->shuttingDown) return;
    RA::Client::onIdle(*s);
  }

  auto pumpServerResponses(RetroAchievements& ra) -> void override {
    auto* s = ra.state();
    if(!s) return;
    RA::Http::pump(*s);
  }

  auto setEnabled(RetroAchievements& ra, bool enabled) -> void override {
    auto* s = ra.state();
    if(!RA::alive(s)) return;

    s->enabled = enabled;
    settings.general.retroAchievements = enabled;
    if(RA::Debug::logging()) {
      RA::print({"RetroAchievements ", enabled ? "enabled" : "disabled"});
    }

    if(!enabled) {
      RA::invalidateAuth(*s);
      RA::abortLoginAsync(*s);
      RA::Client::teardownSession(*s, emulator != nullptr);
      return;
    }

    rc_client_set_hardcore_enabled(s->client, settings.achievements.hardcore ? 1 : 0);
    s->session.hardcore = rc_client_get_hardcore_enabled(s->client);
    if(!s->auth.hasUser) RA::Auth::loadTokenLogin(*s);
    if(s->auth.hasUser && emulator && s->session.gameLoaded) RA::Client::beginGameLoad(*s);
  }

  auto enabled(const RetroAchievements& ra) const -> bool override {
    auto* s = ra.state();
    return s && s->enabled;
  }

  auto startupSetEncoreEnabled(RetroAchievements& ra, bool enabled) -> void override {
    ra._startupEncoreEnabled = enabled;
    if(auto* s = ra.state()) {
      if(!s->client) return;
      s->session.encoreMode = enabled;
      rc_client_set_encore_mode_enabled(s->client, enabled ? 1 : 0);
    }
  }

  auto startupLoginWithPassword(RetroAchievements& ra, const string& username, const string& password) -> void override {
    ra._startupLoginRequested = username && password;
    ra._startupLoginUsername = username;
    ra._startupLoginPassword = password;
  }

  auto startupLogout(RetroAchievements& ra) -> void override {
    ra._startupLogoutRequested = true;
  }

  auto loginWithPassword(RetroAchievements& ra, const string& username, const string& password) -> void override {
    auto* s = ra.state();
    if(!s) return;
    RA::Auth::loginWithPassword(*s, username, password);
  }

  auto logout(RetroAchievements& ra) -> void override {
    auto* s = ra.state();
    if(!s) return;
    RA::Auth::logout(*s);
  }

  auto hardcore(const RetroAchievements& ra) const -> bool override {
    auto* s = ra.state();
    return s && s->session.hardcore;
  }

  auto enableHardcore(RetroAchievements& ra, bool enabled) -> void override {
    settings.achievements.hardcore = enabled;
    if(enabled && settings.debugServer.enabled) {
      settings.debugServer.enabled = false;
      nall::GDB::server.close();
    }

    auto* s = ra.state();
    if(!s || !s->client) {
      return;
    }

    rc_client_set_hardcore_enabled(s->client, enabled ? 1 : 0);
    s->session.hardcore = rc_client_get_hardcore_enabled(s->client);

    if(enabled && program.toolsWindowConstructed) {
      toolsWindow.closeHardcoreRestrictedPanels();
    }
  }

  auto confirmDisableHardcore(RetroAchievements& ra, const string& reason) -> bool override {
    if(!hardcore(ra)) return true;

    auto response = MessageDialog()
      .setTitle(ares::Name)
      .setText({"This action will disable Hardcore Mode. Continue?\n\nAction: ", reason})
      .setAlignment(presentation)
      .question();

    if(response != "Yes") return false;
    enableHardcore(ra, false);
    program.showMessage({"[RA] Hardcore disabled: ", reason});
    return true;
  }

  auto isToolPanelBlockedInHardcore(const RetroAchievements& ra, const string& panelName) const -> bool override {
    if(!hardcore(ra)) return false;
    return panelName == "Cheats" || panelName == "Memory" || panelName == "Tracer" || panelName == "Graphics";
  }

  auto hasUser(const RetroAchievements& ra) const -> bool override {
    auto* s = ra.state();
    return s && s->auth.hasUser;
  }

  auto username(const RetroAchievements& ra) const -> string override {
    auto* s = ra.state();
    return s ? s->auth.username : string{};
  }

  auto userScore(const RetroAchievements& ra) const -> u32 override {
    auto* s = ra.state();
    if(!s || !s->client) return 0;
    if(auto user = rc_client_get_user_info(s->client)) return user->score;
    return 0;
  }

  auto avatarUrl(const RetroAchievements& ra) const -> string override {
    auto* s = ra.state();
    if(!s || !s->client) return {};
    if(auto user = rc_client_get_user_info(s->client)) return user->avatar_url ? string{user->avatar_url} : string{};
    return {};
  }

  auto sessionReady(const RetroAchievements& ra) const -> bool override {
    auto* s = ra.state();
    return s && s->session.ready;
  }

  auto supported(const RetroAchievements&) const -> bool override {
    return RA::Platform::available();
  }
};

#else

struct RetroAchievements::NullBackend final : RetroAchievements::Backend {
  auto initialize(RetroAchievements&) -> void override {}
  auto shutdown(RetroAchievements&) -> void override {}

  auto gameLoaded(RetroAchievements&) -> void override {}
  auto gameUnloaded(RetroAchievements&) -> void override {}
  auto reset(RetroAchievements&) -> void override {}
  auto stateSaved(RetroAchievements&, u32) -> void override {}
  auto stateLoaded(RetroAchievements&, u32) -> void override {}
  auto frame(RetroAchievements&) -> void override {}
  auto idle(RetroAchievements&) -> void override {}
  auto pumpServerResponses(RetroAchievements&) -> void override {}

  auto setEnabled(RetroAchievements&, bool) -> void override {}
  auto enabled(const RetroAchievements&) const -> bool override { return false; }
  auto startupSetEncoreEnabled(RetroAchievements&, bool) -> void override {}
  auto startupLoginWithPassword(RetroAchievements&, const string&, const string&) -> void override {}
  auto startupLogout(RetroAchievements&) -> void override {}
  auto loginWithPassword(RetroAchievements&, const string&, const string&) -> void override {}
  auto logout(RetroAchievements&) -> void override {}
  auto hardcore(const RetroAchievements&) const -> bool override { return false; }
  auto enableHardcore(RetroAchievements&, bool) -> void override {}
  auto confirmDisableHardcore(RetroAchievements&, const string&) -> bool override { return true; }
  auto isToolPanelBlockedInHardcore(const RetroAchievements&, const string&) const -> bool override { return false; }

  auto hasUser(const RetroAchievements&) const -> bool override { return false; }
  auto username(const RetroAchievements&) const -> string override { return {}; }
  auto userScore(const RetroAchievements&) const -> u32 override { return 0; }
  auto avatarUrl(const RetroAchievements&) const -> string override { return {}; }
  auto sessionReady(const RetroAchievements&) const -> bool override { return false; }
  auto supported(const RetroAchievements&) const -> bool override { return false; }
};

#endif

auto RetroAchievements::makeBackend() -> std::unique_ptr<Backend> {
#if ARES_ENABLE_RCHEEVOS
  return std::make_unique<RetroAchievements::RealBackend>();
#else
  return std::make_unique<RetroAchievements::NullBackend>();
#endif
}

RetroAchievements retroAchievements;

RetroAchievements::RetroAchievements() : _backend(RetroAchievements::makeBackend()) {}
RetroAchievements::~RetroAchievements() = default;

auto RetroAchievements::state() -> RA::State* {
  return _state.get();
}

auto RetroAchievements::state() const -> const RA::State* {
  return _state.get();
}

auto RetroAchievements::initialize() -> void {
  _backend->initialize(*this);
}

auto RetroAchievements::shutdown() -> void {
  _backend->shutdown(*this);
}

auto RetroAchievements::gameLoaded() -> void {
  _backend->gameLoaded(*this);
}

auto RetroAchievements::gameUnloaded() -> void {
  _backend->gameUnloaded(*this);
}

auto RetroAchievements::reset() -> void {
  _backend->reset(*this);
}

auto RetroAchievements::stateSaved(u32 slot) -> void {
  _backend->stateSaved(*this, slot);
}

auto RetroAchievements::stateLoaded(u32 slot) -> void {
  _backend->stateLoaded(*this, slot);
}

auto RetroAchievements::frame() -> void {
  _backend->frame(*this);
}

auto RetroAchievements::idle() -> void {
  _backend->idle(*this);
}

auto RetroAchievements::pumpServerResponses() -> void {
  _backend->pumpServerResponses(*this);
}

auto RetroAchievements::setEnabled(bool enabled) -> void {
  _backend->setEnabled(*this, enabled);
}

auto RetroAchievements::enabled() const -> bool {
  return _backend->enabled(*this);
}

auto RetroAchievements::startupSetEncoreEnabled(bool enabled) -> void {
  _backend->startupSetEncoreEnabled(*this, enabled);
}

auto RetroAchievements::startupLoginWithPassword(const string& username, const string& password) -> void {
  _backend->startupLoginWithPassword(*this, username, password);
}

auto RetroAchievements::startupLogout() -> void {
  _backend->startupLogout(*this);
}

auto RetroAchievements::loginWithPassword(const string& username, const string& password) -> void {
  _backend->loginWithPassword(*this, username, password);
}

auto RetroAchievements::logout() -> void {
  _backend->logout(*this);
}

auto RetroAchievements::hardcore() const -> bool {
  return _backend->hardcore(*this);
}

auto RetroAchievements::enableHardcore(bool enabled) -> void {
  _backend->enableHardcore(*this, enabled);
}

auto RetroAchievements::confirmDisableHardcore(const string& reason) -> bool {
  return _backend->confirmDisableHardcore(*this, reason);
}

auto RetroAchievements::isToolPanelBlockedInHardcore(const string& panelName) const -> bool {
  return _backend->isToolPanelBlockedInHardcore(*this, panelName);
}

auto RetroAchievements::hasUser() const -> bool {
  return _backend->hasUser(*this);
}

auto RetroAchievements::username() const -> string {
  return _backend->username(*this);
}

auto RetroAchievements::userScore() const -> u32 {
  return _backend->userScore(*this);
}

auto RetroAchievements::avatarUrl() const -> string {
  return _backend->avatarUrl(*this);
}

auto RetroAchievements::sessionReady() const -> bool {
  return _backend->sessionReady(*this);
}

auto RetroAchievements::supported() const -> bool {
  return _backend->supported(*this);
}
