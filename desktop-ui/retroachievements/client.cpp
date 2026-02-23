#include "../desktop-ui.hpp"
#include "retroachievements.hpp"

#if ARES_ENABLE_RCHEEVOS

#include <memory>

extern "C" {
#include <rcheevos/rcheevos/include/rc_client.h>
#include <rcheevos/rcheevos/include/rc_consoles.h>
}

namespace {

auto isWarningAchievement(const rc_client_achievement_t* achievement) -> bool {
  if(!achievement) return false;
  auto title = achievement->title ? string{achievement->title} : string{};
  return achievement->id == 101000001 || (achievement->points == 0 && title.beginsWith("Warning:"));
}

auto countVisibleAchievements(rc_client_t* client) -> u32 {
  if(!client) return 0;
  u32 count = 0;
  if(auto* list = rc_client_create_achievement_list(
       client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE
     )) {
    for(u32 bucketIndex : range(list->num_buckets)) {
      auto& bucket = list->buckets[bucketIndex];
      for(u32 index : range(bucket.num_achievements)) {
        auto* achievement = bucket.achievements[index];
        if(achievement && !isWarningAchievement(achievement)) count++;
      }
    }
    rc_client_destroy_achievement_list(list);
  }
  return count;
}

auto popupAchievementCounts(rc_client_t* client, u32& unlocked, u32& total) -> bool {
  unlocked = 0;
  total = 0;
  if(!client) return false;

  auto* list = rc_client_create_achievement_list(
    client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE
  );
  if(!list) return false;

  auto unlockBit = rc_client_get_hardcore_enabled(client)
    ? RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE
    : RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE;

  for(u32 bucketIndex : range(list->num_buckets)) {
    const auto& bucket = list->buckets[bucketIndex];
    for(u32 index : range(bucket.num_achievements)) {
      auto* achievement = bucket.achievements[index];
      if(!achievement || isWarningAchievement(achievement)) continue;
      total++;
      if(achievement->unlocked & unlockBit) unlocked++;
    }
  }

  rc_client_destroy_achievement_list(list);
  return true;
}

auto countVisibleLeaderboards(rc_client_t* client) -> u32 {
  if(!client) return 0;
  u32 count = 0;
  if(auto* list = rc_client_create_leaderboard_list(client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE)) {
    for(u32 bucketIndex : range(list->num_buckets)) count += list->buckets[bucketIndex].num_leaderboards;
    rc_client_destroy_leaderboard_list(list);
  }
  return count;
}

struct LoadCallbackData {
  u64 generation = 0;
};

auto osdEnabled() -> bool {
  return settings.general.retroAchievements && settings.achievements.osdEnable;
}

auto achievementsOsdEnabled() -> bool {
  return osdEnabled() && settings.achievements.osdAchievements;
}

auto progressOsdEnabled() -> bool {
  return osdEnabled() && settings.achievements.osdProgress;
}

auto messagesOsdEnabled() -> bool {
  return osdEnabled() && settings.achievements.osdMessages;
}

auto leaderboardsOsdEnabled() -> bool {
  return osdEnabled() && settings.achievements.osdLeaderboards;
}

auto osdAnchor() -> Program::ToastAnchor {
  auto position = settings.achievements.osdPosition;
  if(position == "Top Left") return Program::ToastAnchor::TopLeft;
  if(position == "Top Center") return Program::ToastAnchor::TopCenter;
  if(position == "Top Right") return Program::ToastAnchor::TopRight;
  if(position == "Middle Left") return Program::ToastAnchor::MiddleLeft;
  if(position == "Middle Center") return Program::ToastAnchor::MiddleCenter;
  if(position == "Middle Right") return Program::ToastAnchor::MiddleRight;
  if(position == "Bottom Center") return Program::ToastAnchor::BottomCenter;
  if(position == "Bottom Right") return Program::ToastAnchor::BottomRight;
  return Program::ToastAnchor::BottomLeft;
}

auto osdVisibleDurationMs() -> u64 {
  return settings.achievements.osdDurationMs;
}

auto osdFadeInDurationMs() -> u64 {
  return settings.achievements.osdFadeInMs;
}

auto osdFadeOutDurationMs() -> u64 {
  return settings.achievements.osdFadeOutMs;
}

auto osdAccentColor() -> u32 {
  auto color = settings.achievements.osdAccentColor;
  if(color == "Blue") return 0x2c7be5;
  if(color == "Cyan") return 0x1ccad8;
  if(color == "Fuchsia") return 0xd63384;
  if(color == "Green") return 0x2f9e44;
  if(color == "Lime") return 0x74b816;
  if(color == "Orange") return 0xf08c00;
  if(color == "Red") return 0xe03131;
  if(color == "White") return 0xe9ecef;
  if(color == "Yellow") return 0xfab005;

  // Backward compatibility for any old hex value already saved in settings.
  if(color.beginsWith("#")) color = string{(const char*)color + 1};
  if(color.size() == 6) return color.hex();
  return 0x2c7be5;
}

auto showRA(const string& message) -> void {
  program.showMessage({"[RA] ", message});
}

auto copyToastIcon(image icon, u32& iconWidth, u32& iconHeight) -> std::vector<u32> {
  iconWidth = 0;
  iconHeight = 0;
  if(!icon || icon.width() == 0 || icon.height() == 0) return {};

  icon.transform(false, 32, 255u << 24, 255u << 16, 255u << 8, 255u << 0);
  iconWidth = icon.width();
  iconHeight = icon.height();

  std::vector<u32> pixels;
  pixels.resize(iconWidth * iconHeight);
  for(u32 y : range(iconHeight)) {
    for(u32 x : range(iconWidth)) {
      const u8* sp = (const u8*)icon.data() + y * icon.pitch() + x * icon.stride();
      u64 color = icon.read(sp);

      u64 a = (color & icon.alpha().mask()) >> icon.alpha().shift();
      u64 r = (color & icon.red().mask()) >> icon.red().shift();
      u64 g = (color & icon.green().mask()) >> icon.green().shift();
      u64 b = (color & icon.blue().mask()) >> icon.blue().shift();

      a = image::normalize(a, icon.alpha().depth(), 8);
      r = image::normalize(r, icon.red().depth(), 8);
      g = image::normalize(g, icon.green().depth(), 8);
      b = image::normalize(b, icon.blue().depth(), 8);

      pixels[y * iconWidth + x] = (u32)((a << 24) | (r << 16) | (g << 8) | b);
    }
  }

  return pixels;
}

auto showToastWithImage(const string& title, const string& description, image icon) -> void {
  u32 iconWidth = 0;
  u32 iconHeight = 0;
  auto pixels = copyToastIcon(icon, iconWidth, iconHeight);
  program.showToast(
    title, description, pixels, iconWidth, iconHeight, osdAnchor(),
    osdVisibleDurationMs(), osdFadeInDurationMs(), osdFadeOutDurationMs(), osdAccentColor()
  );
}

auto showToast(const string& title) -> void {
  program.showToast(
    title, {}, {}, 0, 0, osdAnchor(),
    osdVisibleDurationMs(), osdFadeInDurationMs(), osdFadeOutDurationMs(), osdAccentColor()
  );
}

auto showToast(const string& title, const string& description) -> void {
  program.showToast(
    title, description, {}, 0, 0, osdAnchor(),
    osdVisibleDurationMs(), osdFadeInDurationMs(), osdFadeOutDurationMs(), osdAccentColor()
  );
}

auto showProgressToastWithImage(const string& progress, image icon) -> void {
  u32 iconWidth = 0;
  u32 iconHeight = 0;
  auto pixels = copyToastIcon(icon, iconWidth, iconHeight);
  program.showProgressToast(
    progress, pixels, iconWidth, iconHeight, osdAnchor(),
    osdVisibleDurationMs(), osdFadeInDurationMs(), osdFadeOutDurationMs(), osdAccentColor()
  );
}

auto onLoadGameResult(int result, const char* errorMessage, rc_client_t* client, void* userdata) -> void {
  std::unique_ptr<LoadCallbackData> callbackData{(LoadCallbackData*)userdata};
  auto* s = RA::stateFromClient(client);
  if(!s || s->shuttingDown) return;
  s->session.loadHandle = nullptr;

  if(callbackData && callbackData->generation != s->session.loadGeneration.load()) {
    return;
  }

  // if(RA::Debug::logging()) {
  //   RA::Debug::print({"Load game result=", result, " error=", errorMessage ? errorMessage : "<none>"});
  // }

  s->session.pendingRomData.clear();
  if(result != RC_OK) {
    RA::clearSession(*s);
    s->session.gameLoaded = emulator != nullptr;
    RA::StateIO::clear(*s);
    RA::print({"Game load failed: ", errorMessage ? errorMessage : "unknown error"});
    return;
  }

  s->session.ready = rc_client_is_game_loaded(client);
  s->session.hardcore = rc_client_get_hardcore_enabled(client);
  s->session.hasAchievements = rc_client_has_achievements(client);
  s->session.hasLeaderboards = rc_client_has_leaderboards(client);
  s->session.hasVisibleAchievements = countVisibleAchievements(client) > 0;
  s->session.hasVisibleLeaderboards = countVisibleLeaderboards(client) > 0;

  if(!s->session.hasVisibleAchievements && !s->session.hasVisibleLeaderboards && s->session.hardcore) {
    rc_client_set_hardcore_enabled(client, 0);
    s->session.hardcore = rc_client_get_hardcore_enabled(client);
    RA::print("Hardcore disabled for this game: this specific game version has no achievements or leaderboards");
  }

  RA::StateIO::applyPendingAfterGameLoad(*s);
  s->session.initiallyUnlockedIds.clear();
  if(auto* list = rc_client_create_achievement_list(
       client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE
     )) {
    for(u32 bucketIndex : range(list->num_buckets)) {
      auto& bucket = list->buckets[bucketIndex];
      for(u32 index : range(bucket.num_achievements)) {
        auto* achievement = bucket.achievements[index];
        if(achievement && achievement->unlocked != RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE) {
          s->session.initiallyUnlockedIds.insert(achievement->id);
        }
      }
    }
    rc_client_destroy_achievement_list(list);
  }

  if(auto game = rc_client_get_game_info(client)) {
    RA::Images::prefetchGameImage(game);
    program.pendingRetroAchievementsUiRefresh = true;
    if(RA::Debug::logging()) {
      RA::Debug::print({"Loaded game title=", game->title ? game->title : "<null>"});
      RA::Debug::print({"Loaded game id=", game->id, " hash=", game->hash ? game->hash : "<none>"});
      if(game->hash) RA::Debug::print({"Generated RA hash=", game->hash});
      rc_client_user_game_summary_t summary{};
      rc_client_get_user_game_summary(client, &summary);
      RA::Debug::print({
        "User summary unlocked=", summary.num_unlocked_achievements,
        " core=", summary.num_core_achievements,
        " unofficial=", summary.num_unofficial_achievements,
        " unsupported=", summary.num_unsupported_achievements
      });
    }
    rc_client_user_game_summary_t summary{};
    rc_client_get_user_game_summary(client, &summary);
    u32 unlocked = 0;
    u32 total = 0;
    if(!popupAchievementCounts(client, unlocked, total)) {
      unlocked = summary.num_unlocked_achievements;
      total = summary.num_core_achievements + summary.num_unofficial_achievements;
    }
    auto title = game->title ? string{game->title} : string{"Game loaded"};
    string progress = {unlocked, "/", total, " achievements unlocked"};
    showRA({"Game loaded: ", title});
    if(messagesOsdEnabled()) showToastWithImage(title, progress, RA::Images::fetchGameImage(game, 64));
    RA::print({"Game loaded: ", game->title ? game->title : "unknown"});
  } else {
    program.pendingRetroAchievementsUiRefresh = true;
    showRA("Game loaded");
    if(messagesOsdEnabled()) showToast("Game loaded");
    RA::print("Game loaded");
  }
}

auto onEvent(const rc_client_event_t* event, rc_client_t* client) -> void {
  if(!event) return;

  auto* s = RA::stateFromClient(client);
  if(s && s->shuttingDown) return;

  switch(event->type) {
  case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
    if(event->achievement) {
      auto id = event->achievement->id;
      auto title = event->achievement->title ? string{event->achievement->title} : string{"Untitled"};
      auto description = event->achievement->description ? string{event->achievement->description} : string{};
      bool unsupportedWarning = id == 101000001 || title.beginsWith("Warning: Unknown Emulator");
      if(unsupportedWarning) {
        if(s && !s->session.warnedUnsupportedBuild) {
          s->session.warnedUnsupportedBuild = true;
          showRA("Unsupported emulator build detected. Hardcore cheevos cannot unlock, softcore only");
          if(messagesOsdEnabled()) {
            showToast("Unsupported emulator build", "Hardcore achievements cannot unlock on this build");
          }
          RA::print("notice: unsupported emulator build detected. Hardcore cheevos cannot unlock, softcore only");
        }
        break;
      }

      if(s && !s->session.encoreMode && s->session.initiallyUnlockedIds.contains(id)) {
        break;
      }

      bool isEncoreUnlock = s && s->session.encoreMode && s->session.initiallyUnlockedIds.contains(id);
      if(isEncoreUnlock) {
        showRA({"Encore achievement unlocked: ", title});
        if(achievementsOsdEnabled()) showToastWithImage(title, description, RA::Images::fetchAchievementBadge(event->achievement, true, 64));
        RA::print({
          "notification: encore achievement unlocked id=", id, " title=\"", title, "\" points=", event->achievement->points
        });
      } else {
        showRA({"Achievement unlocked: ", title});
        if(achievementsOsdEnabled()) showToastWithImage(title, description, RA::Images::fetchAchievementBadge(event->achievement, true, 64));
        RA::print({"notification: achievement unlocked id=", id, " title=\"", title, "\" points=", event->achievement->points});
      }
      if(description) RA::print({"notification: description=\"", description, "\""});
      if(s) s->session.initiallyUnlockedIds.insert(id);
    }
    break;
  case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
  case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE:
    if(event->achievement && RA::Debug::progress()) {
      RA::Debug::print({
        "progress: id=", event->achievement->id,
        " title=\"", event->achievement->title ? event->achievement->title : "Untitled", "\"",
        " measured=\"", event->achievement->measured_progress[0] ? event->achievement->measured_progress : "", "\""
      });
    }
    break;
  case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
  case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
    if(event->achievement) {
      auto progress = event->achievement->measured_progress[0] ? string{event->achievement->measured_progress} : string{};
      if(RA::Debug::progress()) {
        RA::Debug::print({
          "progress: id=", event->achievement->id,
          " title=\"", event->achievement->title ? event->achievement->title : "Untitled", "\"",
          " measured=\"", progress, "\""
        });
      }
      if(progress && progressOsdEnabled()) {
        showProgressToastWithImage(progress, RA::Images::fetchAchievementBadge(event->achievement, false, 64));
      }
    }
    break;
  case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE:
    if(event->achievement && RA::Debug::progress()) {
      RA::Debug::print({
        "progress: id=", event->achievement->id,
        " title=\"", event->achievement->title ? event->achievement->title : "Untitled", "\"",
        " measured=\"", event->achievement->measured_progress[0] ? event->achievement->measured_progress : "", "\""
      });
    }
    program.hideProgressToast();
    break;
  case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
  case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
  case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
  case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
    if(event->leaderboard) {
      auto title = event->leaderboard->title ? string{event->leaderboard->title} : string{"Leaderboard"};
      if(event->type == RC_CLIENT_EVENT_LEADERBOARD_STARTED) {
        showRA({"Leaderboard started: ", title});
        if(leaderboardsOsdEnabled()) showToast({"Leaderboard started: ", title});
      } else if(event->type == RC_CLIENT_EVENT_LEADERBOARD_FAILED) {
        showRA({"Leaderboard failed: ", title});
        if(leaderboardsOsdEnabled()) showToast({"Leaderboard failed: ", title});
      } else if(event->type == RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED) {
        auto score = event->leaderboard->tracker_value ? string{event->leaderboard->tracker_value} : string{};
        if(score) {
          showRA({"Leaderboard submitted: ", title, " (", score, ")"});
          if(leaderboardsOsdEnabled()) showToast({"Leaderboard submitted: ", title}, score);
        } else {
          showRA({"Leaderboard submitted: ", title});
          if(leaderboardsOsdEnabled()) showToast({"Leaderboard submitted: ", title});
        }
      }
    }
    break;
  case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
  case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE:
    if(event->leaderboard_tracker && RA::Debug::progress()) {
      RA::Debug::print({
        "leaderboard tracker: id=", event->leaderboard_tracker->id,
        " value=\"", event->leaderboard_tracker->display[0] ? event->leaderboard_tracker->display : "", "\""
      });
    }
    break;
  case RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD:
    if(event->leaderboard_scoreboard) {
      string result = {
        "Leaderboard result: rank #", event->leaderboard_scoreboard->new_rank,
        " score=", event->leaderboard_scoreboard->submitted_score,
        " best=", event->leaderboard_scoreboard->best_score
      };
      showRA(result);
      if(leaderboardsOsdEnabled()) showToast(result);
    }
    break;
  case RC_CLIENT_EVENT_GAME_COMPLETED:
    if(auto game = rc_client_get_game_info(client)) {
      auto title = game->title ? string{game->title} : string{"Game"};
      showRA({"Completed: ", title});
      if(messagesOsdEnabled()) showToast({"Completed: ", title});
    }
    break;
  case RC_CLIENT_EVENT_SERVER_ERROR:
    if(event->server_error && event->server_error->error_message) {
      showRA({"Server error: ", event->server_error->error_message});
      if(messagesOsdEnabled()) showToast("RetroAchievements server error", event->server_error->error_message);
      RA::print({"Server error: ", event->server_error->error_message});
    } else {
      showRA("Server error");
      if(messagesOsdEnabled()) showToast("RetroAchievements server error");
      RA::print("Server error");
    }
    break;
  case RC_CLIENT_EVENT_DISCONNECTED:
    showRA("Disconnected from RetroAchievements");
    if(messagesOsdEnabled()) showToast("Disconnected from RetroAchievements");
    RA::print("Disconnected from RetroAchievements");
    break;
  case RC_CLIENT_EVENT_RECONNECTED:
    showRA("Reconnected to RetroAchievements");
    if(messagesOsdEnabled()) showToast("Reconnected to RetroAchievements");
    RA::print("Reconnected to RetroAchievements");
    break;
  case RC_CLIENT_EVENT_SUBSET_COMPLETED:
    if(event->subset && event->subset->title) {
      showRA({"Completed subset: ", event->subset->title});
      if(messagesOsdEnabled()) showToast({"Completed subset: ", event->subset->title});
    } else {
      showRA("Completed subset");
      if(messagesOsdEnabled()) showToast("Completed subset");
    }
    break;
  default:
    break;
  }
}

auto readMemory(u32 address, u8* buffer, u32 size, rc_client_t* client) -> u32 {
  auto* s = RA::stateFromClient(client);
  if(!s || s->shuttingDown || !s->enabled || !emulator) return 0;
  if(!RA::Platform::supports(*emulator)) return 0;
  auto read = RA::Platform::readMemory(address, buffer, size);
  RA::Debug::trackMemoryRead(*s, address, size, read);
  RA::Debug::dumpPerSecondMemorySnapshot(*s);
  return read;
}

}

namespace RA::Client {

auto create(State& s, RetroAchievements* owner) -> void {
  s.client = rc_client_create(readMemory, RA::Http::serverCall);
  if(!s.client) return;

  rc_hash_callbacks_t hashCallbacks{};
  RA::Platform::configureHashCallbacks(hashCallbacks);
  rc_client_set_hash_callbacks(s.client, &hashCallbacks);
  rc_client_set_userdata(s.client, owner);
  rc_client_set_event_handler(s.client, onEvent);
  rc_client_set_allow_background_memory_reads(s.client, 0);
}

auto destroy(State& s) -> void {
  if(!s.client) return;
  rc_client_destroy(s.client);
  s.client = nullptr;
}

// Centralized session teardown for unload/logout/disable/shutdown paths.
auto teardownSession(State& s, bool gameStillLoaded, bool unloadClientGame) -> void {
  abortLoadAsync(s);
  clearSession(s);
  s.session.gameLoaded = gameStillLoaded;
  invalidate(s);
  Http::clearQueues(s);
  StateIO::clear(s);
  if(unloadClientGame && s.client) rc_client_unload_game(s.client);
}

auto beginGameLoad(State& s) -> void {
  if(!alive(&s) || !emulator) return;

  s.session.loadGeneration.fetch_add(1);
  abortLoadAsync(s);
  s.session.pendingRomData.clear();
  s.session.ready = false;
  s.session.hasAchievements = false;
  s.session.hasLeaderboards = false;
  s.session.hasVisibleAchievements = false;
  s.session.hasVisibleLeaderboards = false;
  rc_client_set_hardcore_enabled(s.client, settings.achievements.hardcore ? 1 : 0);
  s.session.hardcore = rc_client_get_hardcore_enabled(s.client);

  if(!s.enabled) return;
  if(!s.auth.hasUser) {
    if(s.auth.pendingLogin) {
      if(Debug::logging()) Debug::print("Login pending; game identification will run after login completes");
    } else {
      if(Debug::logging()) Debug::print("Not logged in; skipping game identification");
    }
    return;
  }
  if(!RA::Platform::supports(*emulator)) {
    if(Debug::logging()) Debug::print({"RetroAchievements is currently unsupported for this system: ", emulator->name});
    return;
  }

  auto activeId = RA::Platform::consoleId(*emulator);
  if(activeId == RC_CONSOLE_UNKNOWN) {
    if(Debug::logging()) Debug::print("Could not determine RA console id");
    return;
  }
  Debug::onGameLoaded(s, *emulator, activeId);

  auto path = RA::Platform::filePath(*emulator);
  auto generation = s.session.loadGeneration.load();
  rc_client_set_encore_mode_enabled(s.client, s.session.encoreMode ? 1 : 0);

  if(!RA::Platform::romData(*emulator, s.session.pendingRomData) || s.session.pendingRomData.empty()) {
    if(!path) {
      if(Debug::logging()) Debug::print("Could not read ROM data or file path for identification");
      return;
    }
    if(Debug::logging()) Debug::print({"Identifying game from file path: ", path});
    s.session.loadHandle = rc_client_begin_identify_and_load_game(
      s.client,
      activeId,
      path,
      nullptr,
      0,
      onLoadGameResult,
      new LoadCallbackData{generation}
    );
    return;
  }

  // rcheevos does not support RC_CONSOLE_ARCADE in rc_hash_from_buffer.
  // Arcade hashes are filename-based, so force path identification.
  if(activeId == RC_CONSOLE_ARCADE && path) {
    if(Debug::logging()) Debug::print({"Identifying arcade game from file path: ", path});
    s.session.loadHandle = rc_client_begin_identify_and_load_game(
      s.client,
      activeId,
      path,
      nullptr,
      0,
      onLoadGameResult,
      new LoadCallbackData{generation}
    );
    return;
  }

  if(Debug::logging()) {
    if(path) {
      Debug::print({"Identifying game from ROM data bytes=", s.session.pendingRomData.size(), " path=", path});
    } else {
      Debug::print({"Identifying game from ROM data bytes=", s.session.pendingRomData.size()});
    }
  }
  s.session.loadHandle = rc_client_begin_identify_and_load_game(
    s.client,
    activeId,
    path ? (const char*)path : nullptr,
    s.session.pendingRomData.data(),
    s.session.pendingRomData.size(),
    onLoadGameResult,
    new LoadCallbackData{generation}
  );
}

auto unloadGame(State& s) -> void {
  if(!hasClient(&s)) return;
  teardownSession(s, false);
}

auto onFrame(State& s) -> void {
  if(!active(&s)) return;
  if(s.session.ready) rc_client_do_frame(s.client);
  else rc_client_idle(s.client);
}

auto onIdle(State& s) -> void {
  if(!active(&s)) return;
  rc_client_idle(s.client);
}

auto reset(State& s) -> void {
  if(!hasClient(&s) || !s.enabled || !s.session.ready || s.shuttingDown) return;

  if(s.session.encoreMode) {
    abortLoadAsync(s);
    rc_client_unload_game(s.client);
    s.session.ready = false;
    s.session.pendingRomData.clear();
    beginGameLoad(s);
    return;
  }

  rc_client_reset(s.client);
}

}

#endif
