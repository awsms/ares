#include <deque>

struct Program : ares::Platform {
  enum class ToastAnchor : u8 {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
  };

  enum class ToastKind : u8 {
    Generic,
    Progress,
  };

  auto create() -> void;
  auto main() -> void;
  auto quit() -> void;

  //platform.cpp
  auto attach(ares::Node::Object) -> void override;
  auto detach(ares::Node::Object) -> void override;
  auto pak(ares::Node::Object) -> std::shared_ptr<vfs::directory> override;
  auto event(ares::Event) -> void override;
  auto log(ares::Node::Debugger::Tracer::Tracer tracer, string_view message) -> void override;
  auto status(string_view message) -> void override;
  auto video(ares::Node::Video::Screen, const u32* data, u32 pitch, u32 width, u32 height) -> void override;
  auto refreshRateHint(double refreshRate) -> void override;
  auto audio(ares::Node::Audio::Stream) -> void override;
  auto input(ares::Node::Input::Input) -> void override;
  auto cheat(u32 address) -> maybe<u32> override;

  //load.cpp
  auto identify(const string& filename) -> std::shared_ptr<Emulator>;
  auto load(std::shared_ptr<Emulator> emulator, string location = {}) -> bool;
  auto load(string location) -> bool;
  auto unload() -> void;

  //states.cpp
  auto stateSave(u32 slot) -> bool;
  auto stateLoad(u32 slot) -> bool;
  auto undoStateSave() -> bool;
  auto undoStateLoad() -> bool;
  auto clearUndoStates() -> void;

  //status.cpp
  auto updateMessage() -> void;
  auto clearOsd() -> void;
  auto showMessage(const string&) -> void;
  auto updateToast() -> void;
  auto showToast(const string&, ToastAnchor anchor = ToastAnchor::BottomLeft) -> void;
  auto showToast(
    const string& title, const string& description, const std::vector<u32>& icon, u32 iconWidth, u32 iconHeight,
    ToastAnchor anchor = ToastAnchor::BottomLeft, u64 visibleDurationMs = 5000, u64 fadeInDurationMs = 220,
    u64 fadeOutDurationMs = 700, u32 accentColor = 0x2c7be5
  ) -> void;
  auto showProgressToast(
    const string& progress, const std::vector<u32>& icon, u32 iconWidth, u32 iconHeight,
    ToastAnchor anchor = ToastAnchor::BottomLeft, u64 visibleDurationMs = 5000, u64 fadeInDurationMs = 220,
    u64 fadeOutDurationMs = 700, u32 accentColor = 0x2c7be5
  ) -> void;
  auto hideProgressToast() -> void;
  auto error(const string&) -> void;

  //utility.cpp
  auto pause(bool) -> void;
  auto mute() -> void;
  auto paletteUpdate() -> void;
  auto runAheadUpdate() -> void;
  auto captureScreenshot(const u32* data, u32 pitch, u32 width, u32 height) -> void;
  auto openFile(BrowserDialog&) -> string;
  auto selectFolder(BrowserDialog&) -> string;
  auto saveFile(BrowserDialog& dialog) -> string;

  //drivers.cpp
  auto videoDriverUpdate() -> void;
  auto videoMonitorUpdate() -> void;
  auto videoFormatUpdate() -> void;
  auto videoFullScreenToggle() -> void;
  auto videoPseudoFullScreenToggle() -> void;

  auto audioDriverUpdate() -> void;
  auto audioDeviceUpdate() -> void;
  auto audioFrequencyUpdate() -> void;
  auto audioLatencyUpdate() -> void;

  auto inputDriverUpdate() -> void;

  auto driverInitFailed(nall::string& driver, const char* kind, auto&& updateSettingsWindow) -> void;
  
  bool startFullScreen = false;
  bool startPseudoFullScreen = false;
  bool kiosk = false;
  std::vector<string> startGameLoad;
  bool noFilePrompt = false;
  bool settingsWindowConstructed = false;
  bool gameBrowserWindowConstructed = false;
  bool toolsWindowConstructed = false;

  string startSystem;
  string startShader;
  string startSaveStateSlot;

  std::vector<ares::Node::Video::Screen> screens;
  std::vector<ares::Node::Audio::Stream> streams;

  bool paused = false;
  bool fastForwarding = false;
  bool rewinding = false;
  bool runAhead = false;
  bool requestFrameAdvance = false;
  bool requestScreenshot = false;
  bool keyboardCaptured = false;
  atomic<bool> pendingKioskExit = false;
  atomic<bool> pendingRetroAchievementsUiRefresh = false;

  struct State {
    u32 slot = 1;
    u32 undoSlot = 1;
  } state;

  //rewind.cpp
  struct Rewind {
    enum class Mode : u32 { Playing, Rewinding } mode = Mode::Playing;
    std::vector<serializer> history;
    u32 length = 0;
    u32 frequency = 0;
    u32 counter = 0;
  } rewind;
  auto rewindSetMode(Rewind::Mode) -> void;
  auto rewindReset() -> void;
  auto rewindRun() -> void;

  struct Message {
    u64 timestamp = 0;
    string text;
    u64 generation = 0;
  } message;
  struct Toast {
    u64 timestamp = 0;
    u64 updatedTimestamp = 0;
    string title;
    string description;
    std::vector<u32> icon;
    u32 iconWidth = 0;
    u32 iconHeight = 0;
    ToastAnchor anchor = ToastAnchor::BottomLeft;
    ToastKind kind = ToastKind::Generic;
    u64 visibleDurationMs = 5000;
    u64 fadeInDurationMs = 220;
    u64 fadeOutDurationMs = 700;
    u32 accentColor = 0x2c7be5;
    u64 generation = 0;
  } toast;

  struct ToastOverlay {
    std::vector<u32> pixels;
    u32 width = 0;
    u32 height = 0;
    s32 x = 0;
    s32 y = 0;
  };

  //osd.cpp
  auto rasterizeToastOverlay(u32 targetWidth, u32 targetHeight, const Toast& toast) -> ToastOverlay;

  class Guard;

  std::vector<Message> messages;
  std::deque<Toast> toasts;
  u64 osdGeneration = 1;
  string configuration;
  atomic<u64> vblanksPerSecond = 0;

  bool _isRunning = false;

  /// Mutex used to manage access to the input system. Polling occurs on the main thread while the results are read by the emulation thread.
  std::recursive_mutex inputMutex;
  
private:
  /// Routine used while the emulator thread is waiting for work to complete that modifies its state.
  auto waitForInterrupts() -> void;
  /// The main run loop for the emulator thread spawned on startup.
  auto emulatorRunLoop(uintptr_t) -> void;

  atomic<bool> _quitting = false;
  atomic<bool> _needsResize = false;

  /// Mutex used to manage access to the status message queue.
  std::recursive_mutex _messageMutex;

  /// Mutex used to manage interrupts to the emulator run loop. Acquired when setting either of the `_interruptWaiting` or `_interruptWorking` booleans.
  std::mutex _programMutex;
  /// The emulator run loop condition variable. This variable is signaled to indicate other threads may access the program mutex, as well as to indicate that the emulator run loop can continue.
  std::condition_variable _programConditionVariable;
  /// Boolean indicating whether another thread is waiting to modify the emulator program state.
  std::atomic<bool> _interruptWaiting = false;
  /// Boolean indicating whether a non-emulator worker thread is in the process of modifying the emulator program state.
  bool _interruptWorking = false;
  /// Counter used to allow for possible recursive creation of `Program::Guard` instances (which can happen in some UI paths).
  u32 _interruptDepth = 0;
  /// Thread-local variable used to allow the emulator worker thread to create `Program::Guard` instances without causing a deadlock.
  static inline thread_local bool _programThread = false;
  /// Debug accessor for `_programThread` since debuggers cannot read TLS values correctly.
  NALL_USED auto getProgramThreadValue() -> bool {
    return _programThread;
  }
  /// Unique lock used by `Program::Guard` instances to synchronize interrupts. Generally this should not be used directly, with synchronization happening via `Program::Guard` construction. However, it is still exposed for exceptions such as the exit handler in `Program::quit()`.
  std::unique_lock<std::mutex> lock{_programMutex, std::defer_lock};
};

extern Program program;
