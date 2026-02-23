#if ARES_ENABLE_RCHEEVOS

#include <atomic>
#include <cctype>
#include <curl/curl.h>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#endif

#include "../desktop-ui.hpp"
#include "retroachievements.hpp"

#if ARES_ENABLE_RCHEEVOS

namespace {

struct ImageRequest {
  std::string cachePath;
  std::string url;
  u32 size = 0;
};

struct ImageCache {
  std::mutex mutex;
  std::condition_variable cv;
  std::thread worker;
  std::deque<ImageRequest> queue;
  std::unordered_map<std::string, image> memory;
  std::unordered_set<std::string> pending;
  bool stopping = false;
  bool started = false;
  std::atomic<bool> dirty = false;
} imageCache;

auto rootPath() -> string {
  auto root = string{Path::userData(), "ares/cheevos/"};
  directory::create(root);
  return root;
}

auto badgesPath() -> string {
  auto path = string{rootPath(), "badges/"};
  directory::create(path);
  return path;
}

auto usersPath() -> string {
  auto path = string{rootPath(), "users/"};
  directory::create(path);
  return path;
}

auto gamesPath() -> string {
  auto path = string{rootPath(), "games/"};
  directory::create(path);
  return path;
}

auto sanitizeFileComponent(const string& value) -> std::string {
  std::string output;
  output.reserve(value.size());
  for(auto ch : value) {
    auto uch = (unsigned char)ch;
    if(std::isalnum(uch) || ch == '-' || ch == '_') output.push_back((char)ch);
    else output.push_back('_');
  }
  while(!output.empty() && output.back() == '_') output.pop_back();
  while(!output.empty() && output.front() == '_') output.erase(output.begin());
  if(output.empty()) output = "unknown";
  return output;
}

auto cacheKey(const std::string& path, u32 size) -> std::string {
  return path + "#" + std::to_string(size);
}

auto loadScaledImage(const std::vector<u8>& bytes, u32 size) -> image {
  if(bytes.empty()) return {};
  image loaded{bytes.data(), (u32)bytes.size()};
  if(!loaded) return {};
  if(size) loaded.scale(size, size, true);
  return loaded;
}

auto loadScaledImage(const string& path, u32 size) -> image {
  return loadScaledImage(file::read(path), size);
}

auto cachePathForAchievement(const rc_client_achievement_t* achievement, bool unlocked) -> string {
  if(!achievement || !achievement->badge_name[0]) return {};
  auto badge = sanitizeFileComponent(achievement->badge_name);
  return {badgesPath(), "achievement-", badge.c_str(), unlocked ? ".png" : "_locked.png"};
}

auto cachePathForUser(const string& username) -> string {
  if(!username) return {};
  auto clean = sanitizeFileComponent(username);
  return {usersPath(), "user_", clean.c_str(), ".png"};
}

auto cachePathForGame(const rc_client_game_t* game) -> string {
  if(!game) return {};
  if(game->badge_name && game->badge_name[0]) {
    auto clean = sanitizeFileComponent(game->badge_name);
    return {gamesPath(), "game_", clean.c_str(), ".png"};
  }
  if(game->id) return {gamesPath(), "game_", game->id, ".png"};
  return {};
}

auto downloadBytes(const std::string& url) -> std::vector<u8> {
  std::vector<u8> bytes;
  CURL* curl = curl_easy_init();
  if(!curl) return bytes;

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
    auto* body = (std::vector<u8>*)userdata;
    auto count = size * nmemb;
    body->insert(body->end(), (u8*)ptr, (u8*)ptr + count);
    return count;
  });
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bytes);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "ares/retroachievements-minimal");

  auto result = curl_easy_perform(curl);
  long statusCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
  curl_easy_cleanup(curl);
  if(result != CURLE_OK || statusCode < 200 || statusCode >= 300) bytes.clear();
  return bytes;
}

auto workerLoop() -> void {
  while(true) {
    ImageRequest request;
    {
      std::unique_lock lock{imageCache.mutex};
      imageCache.cv.wait(lock, [] { return imageCache.stopping || !imageCache.queue.empty(); });
      if(imageCache.stopping && imageCache.queue.empty()) return;
      request = std::move(imageCache.queue.front());
      imageCache.queue.pop_front();
    }

    auto bytes = downloadBytes(request.url);
    image decoded;
    if(!bytes.empty()) {
      auto directoryPath = Location::path(request.cachePath.c_str());
      if(directoryPath) directory::create(directoryPath);
      auto temporaryPath = string{request.cachePath.c_str(), ".tmp"};
      if(file::write(temporaryPath, {bytes.data(), bytes.size()})) {
        if(!file::move(temporaryPath, request.cachePath.c_str())) file::remove(temporaryPath);
      } else {
        file::remove(temporaryPath);
      }
      decoded = loadScaledImage(bytes, request.size);
    }

    std::lock_guard lock{imageCache.mutex};
    imageCache.pending.erase(request.cachePath);
    if(decoded) imageCache.memory[cacheKey(request.cachePath, request.size)] = decoded;
    imageCache.dirty = true;
  }
}

auto queueFetch(const string& path, const string& url, u32 size) -> image {
  if(!path || !url) return {};

  auto key = cacheKey((const char*)path, size);
  {
    std::lock_guard lock{imageCache.mutex};
    if(auto it = imageCache.memory.find(key); it != imageCache.memory.end()) return it->second;
  }

  if(file::exists(path)) {
    auto cached = loadScaledImage(path, size);
    if(cached) {
      std::lock_guard lock{imageCache.mutex};
      imageCache.memory[key] = cached;
    }
    return cached;
  }

  {
    std::lock_guard lock{imageCache.mutex};
    if(!imageCache.started || imageCache.pending.contains((const char*)path)) return {};
    imageCache.pending.insert((const char*)path);
    imageCache.queue.push_back({(const char*)path, (const char*)url, size});
  }
  imageCache.cv.notify_one();
  return {};
}

auto achievementImageUrl(const rc_client_achievement_t* achievement, bool unlocked) -> string {
  if(!achievement) return {};
  auto url = unlocked ? achievement->badge_url : achievement->badge_locked_url;
  if(url && url[0]) return url;

  char buffer[512] = {};
  auto state = unlocked ? RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED : RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE;
  if(rc_client_achievement_get_image_url(achievement, state, buffer, sizeof(buffer)) == RC_OK) return buffer;
  return {};
}

auto gameImageUrl(const rc_client_game_t* game) -> string {
  if(!game) return {};
  if(game->badge_url && game->badge_url[0]) return game->badge_url;

  char buffer[512] = {};
  if(rc_client_game_get_image_url(game, buffer, sizeof(buffer)) == RC_OK) return buffer;
  return {};
}

}

namespace RA::Images {

auto start() -> void {
  std::lock_guard lock{imageCache.mutex};
  if(imageCache.started) return;
  imageCache.stopping = false;
  imageCache.started = true;
  imageCache.worker = std::thread(workerLoop);
}

auto stop() -> void {
  {
    std::lock_guard lock{imageCache.mutex};
    if(!imageCache.started) return;
    imageCache.stopping = true;
  }
  imageCache.cv.notify_all();
  if(imageCache.worker.joinable()) imageCache.worker.join();

  std::lock_guard lock{imageCache.mutex};
  imageCache.started = false;
  imageCache.queue.clear();
  imageCache.pending.clear();
}

auto consumeDirty() -> bool {
  return imageCache.dirty.exchange(false);
}

auto fetchAchievementBadge(const rc_client_achievement_t* achievement, bool unlocked, u32 size) -> image {
  return queueFetch(cachePathForAchievement(achievement, unlocked), achievementImageUrl(achievement, unlocked), size);
}

auto fetchUserAvatar(const string& username, const string& url, u32 size) -> image {
  return queueFetch(cachePathForUser(username), url, size);
}

auto fetchGameImage(const rc_client_game_t* game, u32 size) -> image {
  return queueFetch(cachePathForGame(game), gameImageUrl(game), size);
}

auto prefetchUserAvatar(const rc_client_user_t* user) -> void {
  if(!user || !user->username) return;
  auto url = user->avatar_url ? string{user->avatar_url} : string{};
  if(!url) {
    char buffer[512] = {};
    if(rc_client_user_get_image_url(user, buffer, sizeof(buffer)) == RC_OK) url = buffer;
  }
  queueFetch(cachePathForUser(user->username), url, 48);
}

auto prefetchGameImage(const rc_client_game_t* game) -> void {
  fetchGameImage(game, 0);
}

}

#endif
