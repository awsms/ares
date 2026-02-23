auto Program::updateMessage() -> void {
  // This function is called every iteration of the GUI run loop. Acquiring the emulator mutex would incur a severe
  // responsiveness penalty, so use a dedicated mutex for message passing.
  lock_guard<recursive_mutex> messageLock(_messageMutex);
  auto generation = osdGeneration;
  if(message.generation && message.generation != generation) message = {};
  while(!messages.empty() && messages.front().generation != generation) {
    messages.erase(messages.begin());
  }

  if(chrono::millisecond() - message.timestamp >= 2000) {
    message = {};
    if(!messages.empty()) { message = messages.front(); messages.erase(messages.begin()); }
  }

  if(message.text.length() > 0) {
    presentation.statusLeft.setText(message.text);
  } else if(settings.debugServer.enabled) {
    presentation.statusLeft.setText(nall::GDB::server.getStatusText(settings.debugServer.port, settings.debugServer.useIPv4));
  } else if(configuration) {
    presentation.statusLeft.setText(configuration);
  } else {
    presentation.statusLeft.setText();
  }

  if(vblanksPerSecond > 0 && !paused) {
    presentation.statusRight.setText({ vblanksPerSecond.load(), " VPS" });
  }

  if(!emulator) {
    presentation.statusRight.setText("Unloaded");
  }

  if (message.text == "") {
    if (emulator && keyboardCaptured) {
      presentation.statusLeft.setText("Keyboard capture is active");
    }
  }

  
  bool defocused = settings.input.defocus == "Pause" && !ruby::video.fullScreen() && !presentation.focused();
  if(emulator && defocused) message = {chrono::millisecond(), "Paused", generation};
}

auto Program::clearOsd() -> void {
  lock_guard<recursive_mutex> messageLock(_messageMutex);
  osdGeneration++;
  message = {};
  toast = {};
  messages.clear();
  toasts.clear();
}

auto Program::showMessage(const string& text) -> void {
  lock_guard<recursive_mutex> messageLock(_messageMutex);
  messages.push_back({chrono::millisecond(), text, osdGeneration});
  printf("%s\n", (const char*)text);
}

auto Program::updateToast() -> void {
  lock_guard<recursive_mutex> messageLock(_messageMutex);
  auto generation = osdGeneration;

  auto pruneToastQueue = [&] {
    while(!toasts.empty() && toasts.front().generation != generation) {
      toasts.pop_front();
    }
  };
  pruneToastQueue();
  if(toast.generation && toast.generation != generation) {
    toast = {};
  }

  auto hasActiveToast = [&]() -> bool {
    return toast.title.length() > 0 || toast.description.length() > 0;
  };

  if(!hasActiveToast() && !toasts.empty()) {
    toast = toasts.front();
    toast.timestamp = chrono::millisecond();
    toast.updatedTimestamp = toast.timestamp;
    toasts.pop_front();
    pruneToastQueue();
  }

  auto totalLifetimeMs = toast.visibleDurationMs + toast.fadeOutDurationMs;
  if(hasActiveToast() && chrono::millisecond() - toast.updatedTimestamp >= totalLifetimeMs) {
    toast = {};
    pruneToastQueue();
    if(!toasts.empty()) {
      toast = toasts.front();
      toast.timestamp = chrono::millisecond();
      toast.updatedTimestamp = toast.timestamp;
      toasts.pop_front();
      pruneToastQueue();
    }
  }
}

auto Program::showToast(const string& text, ToastAnchor anchor) -> void {
  if(text.length() == 0) return;
  lock_guard<recursive_mutex> messageLock(_messageMutex);
  Toast item;
  item.timestamp = chrono::millisecond();
  item.title = text;
  item.anchor = anchor;
  item.generation = osdGeneration;
  toasts.push_back(std::move(item));
}

auto Program::showToast(
  const string& title, const string& description, const std::vector<u32>& icon, u32 iconWidth, u32 iconHeight, ToastAnchor anchor,
  u64 visibleDurationMs, u64 fadeInDurationMs, u64 fadeOutDurationMs, u32 accentColor
) -> void {
  if(title.length() == 0 && description.length() == 0) return;
  lock_guard<recursive_mutex> messageLock(_messageMutex);
  Toast item;
  item.timestamp = chrono::millisecond();
  item.updatedTimestamp = item.timestamp;
  item.title = title;
  item.description = description;
  item.icon = icon;
  item.iconWidth = iconWidth;
  item.iconHeight = iconHeight;
  item.anchor = anchor;
  item.kind = ToastKind::Generic;
  item.visibleDurationMs = visibleDurationMs;
  item.fadeInDurationMs = fadeInDurationMs;
  item.fadeOutDurationMs = fadeOutDurationMs;
  item.accentColor = accentColor;
  item.generation = osdGeneration;
  toasts.push_back(std::move(item));
}

auto Program::showProgressToast(
  const string& progress, const std::vector<u32>& icon, u32 iconWidth, u32 iconHeight, ToastAnchor anchor,
  u64 visibleDurationMs, u64 fadeInDurationMs, u64 fadeOutDurationMs, u32 accentColor
) -> void {
  if(progress.length() == 0) return;
  lock_guard<recursive_mutex> messageLock(_messageMutex);

  auto apply = [&](Toast& item, bool restartLifetime) {
    // Progress updates should refresh the visible content in place instead of
    // replaying the fade-in animation every time the measured value changes.
    auto now = chrono::millisecond();
    if(restartLifetime) item.timestamp = now;
    item.updatedTimestamp = now;
    item.title = progress;
    item.description = {};
    item.icon = icon;
    item.iconWidth = iconWidth;
    item.iconHeight = iconHeight;
    item.anchor = anchor;
    item.kind = ToastKind::Progress;
    item.visibleDurationMs = visibleDurationMs;
    item.fadeInDurationMs = fadeInDurationMs;
    item.fadeOutDurationMs = fadeOutDurationMs;
    item.accentColor = accentColor;
    item.generation = osdGeneration;
  };

  if(toast.kind == ToastKind::Progress && toast.generation == osdGeneration) {
    apply(toast, false);
  } else {
    for(auto& queued : toasts) {
      if(queued.kind != ToastKind::Progress || queued.generation != osdGeneration) continue;
      apply(queued, false);
      return;
    }

    Toast item;
    apply(item, true);
    toasts.push_back(std::move(item));
  }
}

auto Program::hideProgressToast() -> void {
  lock_guard<recursive_mutex> messageLock(_messageMutex);
  if(toast.kind == ToastKind::Progress) {
    // Progress hide events should dismiss the tracker gently instead of
    // abruptly erasing the currently visible card.
    auto now = chrono::millisecond();
    auto fadeStart = now >= toast.visibleDurationMs ? now - toast.visibleDurationMs : 0;
    if(toast.updatedTimestamp < fadeStart) toast.updatedTimestamp = fadeStart;
  }

  for(auto it = toasts.begin(); it != toasts.end();) {
    if(it->kind == ToastKind::Progress) it = toasts.erase(it);
    else ++it;
  }
}

auto Program::error(const string& text) -> void {
  if(kiosk) {
    fprintf(stderr, "error: %s\n", text.data());
    pendingKioskExit = true;
  } else {
    MessageDialog().setTitle("Error").setText(text).setAlignment(presentation).error();
  }
}
