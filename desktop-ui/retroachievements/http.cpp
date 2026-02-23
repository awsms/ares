#if ARES_ENABLE_RCHEEVOS

#include <curl/curl.h>

#endif

#include "../desktop-ui.hpp"
#include "retroachievements.hpp"

#if ARES_ENABLE_RCHEEVOS

namespace {

constexpr long httpTimeoutSeconds = 20L;

auto notifyServerError(rc_client_server_callback_t callback, void* callbackData) -> void {
  if(!callback) return;
  rc_api_server_response_t response{};
  response.http_status_code = RC_API_SERVER_RESPONSE_CLIENT_ERROR;
  callback(&response, callbackData);
}

auto queueServerError(rc_client_server_callback_t callback, void* callbackData) -> void {
  notifyServerError(callback, callbackData);
}

auto workerLoop(RA::State& s) -> void {
  while(true) {
    RA::HttpRequest request;
    {
      std::unique_lock lock{s.http.mutex};
      s.http.cv.wait(lock, [&] { return s.http.stopping || !s.http.requestQueue.empty(); });
      if(s.http.stopping && s.http.requestQueue.empty()) return;
      request = std::move(s.http.requestQueue.front());
      s.http.requestQueue.pop_front();
    }

    RA::HttpResponse response;
    response.generation = request.generation;
    response.callback = request.callback;
    response.callbackData = request.callbackData;
    response.statusCode = RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;

    CURL* curl = curl_easy_init();
    if(!curl) {
      std::lock_guard lock{s.http.mutex};
      s.http.responseQueue.push_back(std::move(response));
      continue;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, "User-Agent: ares/retroachievements-minimal");

    curl_easy_setopt(curl, CURLOPT_URL, (const char*)request.url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, httpTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
      auto* body = (std::vector<char>*)userdata;
      body->insert(body->end(), ptr, ptr + size * nmemb);
      return size * nmemb;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    if(request.postData) {
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (const char*)request.postData);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)request.postData.size());
    }

    auto result = curl_easy_perform(curl);
    if(result == CURLE_OK) {
      long code = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
      response.statusCode = code ? code : 200;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    std::lock_guard lock{s.http.mutex};
    s.http.responseQueue.push_back(std::move(response));
  }
}

}

namespace RA::Http {

auto start(State& s) -> void {
  s.shuttingDown = false;

  if(curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
    RA::print("Failed to initialize curl");
    s.http.curlGlobalInitialized = false;
    return;
  }
  s.http.curlGlobalInitialized = true;

  {
    std::lock_guard lock{s.http.mutex};
    s.http.stopping = false;
  }
  s.http.thread = std::thread([&s] { workerLoop(s); });
}

auto stop(State& s) -> void {
  {
    std::lock_guard lock{s.http.mutex};
    s.http.stopping = true;
  }
  s.http.cv.notify_all();
  if(s.http.thread.joinable()) s.http.thread.join();

  if(s.http.curlGlobalInitialized) {
    curl_global_cleanup();
    s.http.curlGlobalInitialized = false;
  }
}

auto clearQueues(State& s) -> void {
  std::deque<HttpRequest> droppedRequests;
  std::deque<HttpResponse> droppedResponses;
  {
    std::lock_guard lock{s.http.mutex};
    droppedRequests.swap(s.http.requestQueue);
    droppedResponses.swap(s.http.responseQueue);
  }

  for(auto& request : droppedRequests) {
    notifyServerError(request.callback, request.callbackData);
  }
  for(auto& response : droppedResponses) {
    notifyServerError(response.callback, response.callbackData);
  }
}

auto pump(State& s) -> void {
  std::deque<HttpResponse> responses;
  {
    std::lock_guard lock{s.http.mutex};
    if(s.http.responseQueue.empty()) return;
    responses.swap(s.http.responseQueue);
  }

  if(!alive(&s)) return;

  auto currentGeneration = s.http.generation.load();
  for(auto& item : responses) {
    if(!item.callback) continue;
    rc_api_server_response_t response{};
    response.body = item.body.empty() ? nullptr : item.body.data();
    response.body_length = item.body.size();
    response.http_status_code =
      item.generation == currentGeneration ? (int)item.statusCode : RC_API_SERVER_RESPONSE_CLIENT_ERROR;
    item.callback(&response, item.callbackData);
  }
}

auto serverCall(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callbackData, rc_client_t* client)
  -> void {
  auto* s = RA::stateFromClient(client);
  if(!s || !request || !callback) {
    queueServerError(callback, callbackData);
    return;
  }

  RA::HttpRequest httpRequest;
  httpRequest.generation = s->http.generation.load();
  httpRequest.url = request->url ? request->url : "";
  httpRequest.postData = request->post_data ? request->post_data : "";
  httpRequest.callback = callback;
  httpRequest.callbackData = callbackData;

  {
    std::lock_guard lock{s->http.mutex};
    if(!alive(s) || s->http.stopping) {
      queueServerError(callback, callbackData);
      return;
    }
    s->http.requestQueue.push_back(std::move(httpRequest));
  }
  s->http.cv.notify_one();
}

}

#endif
