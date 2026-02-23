#include "../desktop-ui.hpp"
#include "retroachievements.hpp"

#if ARES_ENABLE_RCHEEVOS

namespace {

struct LoginCallbackData {
  u64 generation = 0;
};

auto loginMessage(const rc_client_user_t& user) -> string {
  auto name = user.display_name ? user.display_name : user.username;
  u32 hardcoreScore = user.score >= user.score_softcore ? user.score - user.score_softcore : 0;
  return {
    "Logged in as ", name, " (", user.score, " pts. SC: ", user.score_softcore, " pts, HC: ", hardcoreScore, " pts)"
  };
}

auto onLoginResult(int result, const char* errorMessage, rc_client_t* client, void* userdata) -> void {
  std::unique_ptr<LoginCallbackData> callbackData{(LoginCallbackData*)userdata};
  auto* s = RA::stateFromClient(client);
  if(!RA::alive(s)) return;
  if(callbackData && callbackData->generation != s->auth.generation.load()) return;

  RA::clearPendingLogin(*s);
  if(result != RC_OK || !rc_client_get_user_info(client)) {
    s->auth.hasUser = false;
    s->auth.username = "";
    s->auth.pendingToken = "";
    RA::clearSession(*s);
    if(result == RC_LOGIN_REQUIRED || result == RC_INVALID_CREDENTIALS || result == RC_EXPIRED_TOKEN) {
      settings.achievements.token = "";
      settings.save();
    }
    RA::print({"Login failed: ", errorMessage ? errorMessage : "unknown error"});
    return;
  }

  auto user = rc_client_get_user_info(client);
  s->auth.hasUser = true;
  s->session.hardcore = rc_client_get_hardcore_enabled(client);
  s->auth.username = user->username ? string{user->username} : string{};
  settings.achievements.username = s->auth.username;
  settings.achievements.token = user->token ? string{user->token} : string{};
  settings.save();
  RA::Images::prefetchUserAvatar(user);
  auto message = loginMessage(*user);
  // RA::print(message);
  program.showMessage({"[RA] ", message});

  if(s->enabled && emulator && s->session.gameLoaded) RA::Client::beginGameLoad(*s);
}

}

namespace RA::Auth {

auto applyStartup(State& s, RetroAchievements& ra) -> void {
  if(s.shuttingDown) return;

  if(ra._startupLogoutRequested) {
    settings.achievements.token = "";
    settings.save();
    ra._startupLogoutRequested = false;
  }

  if(!s.enabled) return;

  if(ra._startupLoginRequested && ra._startupLoginUsername && ra._startupLoginPassword) {
    auto username = ra._startupLoginUsername;
    auto password = ra._startupLoginPassword;
    ra._startupLoginRequested = false;
    ra._startupLoginUsername = {};
    ra._startupLoginPassword = {};
    loginWithPassword(s, username, password);
  } else {
    loadTokenLogin(s);
  }
}

auto loginWithPassword(State& s, const string& username, const string& password) -> void {
  if(!alive(&s)) return;
  if(!username || !password) return;

  s.enabled = true;
  settings.general.retroAchievements = true;
  invalidateAuth(s);
  auto generation = s.auth.generation.load();
  abortLoginAsync(s);
  s.auth.pendingLogin = true;
  s.auth.pendingUsername = username;
  s.auth.pendingToken = "";
  if(Debug::logging()) Debug::print({"Attempting login for user ", username});
  s.auth.loginHandle =
    rc_client_begin_login_with_password(s.client, username, password, onLoginResult, new LoginCallbackData{generation});
}

auto logout(State& s) -> void {
  if(!alive(&s)) return;

  invalidateAuth(s);
  abortLoginAsync(s);
  RA::Client::teardownSession(s, emulator != nullptr, false);
  clearAuth(s);
  rc_client_logout(s.client);
  settings.achievements.token = "";
  settings.save();
  if(Debug::logging()) Debug::print("Logged out");
}

auto loadTokenLogin(State& s) -> void {
  if(!active(&s) || s.auth.hasUser || s.auth.pendingLogin) return;
  auto usernameText = settings.achievements.username;
  auto tokenText = settings.achievements.token;
  if(!usernameText || !tokenText) return;

  invalidateAuth(s);
  auto generation = s.auth.generation.load();
  abortLoginAsync(s);
  s.auth.pendingLogin = true;
  s.auth.pendingUsername = usernameText;
  s.auth.pendingToken = tokenText;
  if(Debug::logging()) Debug::print({"Attempting token login for user ", s.auth.pendingUsername});
  s.auth.loginHandle = rc_client_begin_login_with_token(
    s.client, s.auth.pendingUsername, s.auth.pendingToken, onLoginResult, new LoginCallbackData{generation}
  );
}

}

#endif
