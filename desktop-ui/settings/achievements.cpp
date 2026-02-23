auto AchievementSettings::construct() -> void {
  setCollapsible();
  setVisible(false);

  auto clampOsdTiming = [](const string& value, u32 fallback) -> u32 {
    if(value == "0") return 0;
    auto parsed = value.natural();
    if(parsed == 0) return fallback;
    return parsed;
  };

  integrationLabel.setText("RetroAchievements").setFont(Font().setBold());
  integrationLayout.setAlignment(1).setPadding(12_sx, 0);
  integrationOption.setText("Enable RetroAchievements integration").setChecked(settings.general.retroAchievements).onToggle([&] {
    settings.general.retroAchievements = integrationOption.checked();
    retroAchievements.setEnabled(settings.general.retroAchievements);
    refresh();
  });
  integrationHint.setText("Enable achievements through RetroAchievements.org")
    .setFont(Font().setSize(7.0))
    .setForegroundColor(SystemColor::Sublabel);

  accountLabel.setText("Account").setFont(Font().setBold());

  usernameLayout.setAlignment(1).setPadding(12_sx, 0);
  usernameLayout.setCollapsible();
  usernameLabel.setText("Username:");
  usernameValue.setText(settings.achievements.username).setEditable(true).onChange([&] {
    settings.achievements.username = usernameValue.text();
  });

  passwordLayout.setAlignment(1).setPadding(12_sx, 0);
  passwordLayout.setCollapsible();
  passwordLabel.setText("Password:");
  passwordValue.setEditable(true).setMasked(true);

  accountButtonsLayout.setAlignment(1).setPadding(12_sx, 0);
  accountButtonsLayout.setCollapsible();
  profileTopGap.setCollapsible();
  accountStatus.setForegroundColor(SystemColor::Sublabel);
  profileName.setFont(Font().setBold());
  profilePoints.setForegroundColor(SystemColor::Sublabel);
  profileLayout.setAlignment(1).setPadding(12_sx, 0);
  profileLayout.setCollapsible();
  profileBottomGap.setCollapsible();
  loginButton.setText("Login").onActivate([&] {
    settings.achievements.username = usernameValue.text();
    if(!settings.general.retroAchievements) return;
    if(!settings.achievements.username) {
      return (void)program.showMessage("[RA] Enter a username before logging in.");
    }
    if(!passwordValue.text()) {
      return (void)program.showMessage("[RA] Enter a password to log in.");
    }
    retroAchievements.loginWithPassword(settings.achievements.username, passwordValue.text());
    refresh();
  });

  logoutButton.setText("Logout").onActivate([&] {
    retroAchievements.logout();
    refresh();
  });

  behaviorLabel.setText("Behavior").setFont(Font().setBold());

  encoreLayout.setAlignment(1).setPadding(12_sx, 0);
  encoreOption.setText("Enable Encore achievements").setChecked(settings.achievements.encore).onToggle([&] {
    settings.achievements.encore = encoreOption.checked();
    retroAchievements.startupSetEncoreEnabled(settings.achievements.encore);
    refresh();
  });
  encoreHint.setText("Allows re-earning achievements already unlocked on your account")
    .setFont(Font().setSize(7.0))
    .setForegroundColor(SystemColor::Sublabel);

  hardcoreLayout.setAlignment(1).setPadding(12_sx, 0);
  hardcoreOption.setText("Enable Hardcore mode").setChecked(settings.achievements.hardcore).onToggle([&] {
    if(hardcoreToggleGuard) return;
    auto enableHardcore = hardcoreOption.checked();
    if(enableHardcore == settings.achievements.hardcore) return;

    if(enableHardcore) {
      auto response = MessageDialog()
        .setTitle(ares::Name)
        .setText(
          "Enabling Hardcore will restart the current game.\n\n"
          "While Hardcore is active:\n"
          "- Load states are disabled\n"
          "- Cheats, memory inspector, and tracer are restricted\n"
          "- Rewind and frame advance are disabled\n\n"
          "Continue?"
        )
        .setAlignment(settingsWindow)
        .question();
      if(response != "Yes") {
        hardcoreToggleGuard = true;
        hardcoreOption.setChecked(false);
        hardcoreToggleGuard = false;
        return;
      }
    }

    settings.achievements.hardcore = enableHardcore;
    if(enableHardcore) {
      settings.general.runAhead = false;
      settings.general.rewind = false;
      program.runAheadUpdate();
      program.rewindReset();
      if(settings.debugServer.enabled) debugSettings.enabled.setChecked(false);
    }
    retroAchievements.enableHardcore(enableHardcore);
    if(enableHardcore && emulator) {
      Program::Guard guard;
      emulator->root->power(true);
      retroAchievements.reset();
    }

    refresh();
  });
  hardcoreHint.setText("Hardcore blocks gameplay-altering tools and may require disabling it for debugging")
    .setFont(Font().setSize(7.0))
    .setForegroundColor(SystemColor::Sublabel);

  osdEnableLayout.setAlignment(1).setPadding(12_sx, 0);
  osdEnableOption.setText("Enable RetroAchievements OSD").setChecked(settings.achievements.osdEnable).onToggle([&] {
    settings.achievements.osdEnable = osdEnableOption.checked();
    refresh();
  });
  osdEnableHint.setText("Shows RetroAchievements pop-up notifications during gameplay")
    .setFont(Font().setSize(7.0))
    .setForegroundColor(SystemColor::Sublabel);

  osdAchievementsLayout.setAlignment(1).setPadding(24_sx, 0);
  osdAchievementsOption.setText("Show achievement pop-ups").setChecked(settings.achievements.osdAchievements).onToggle([&] {
    settings.achievements.osdAchievements = osdAchievementsOption.checked();
    refresh();
  });
  osdAchievementsHint.setText("Shows unlock toasts for achievements and encore unlocks")
    .setFont(Font().setSize(7.0))
    .setForegroundColor(SystemColor::Sublabel);

  osdProgressLayout.setAlignment(1).setPadding(24_sx, 0);
  osdProgressOption.setText("Show achievement progress pop-ups").setChecked(settings.achievements.osdProgress).onToggle([&] {
    settings.achievements.osdProgress = osdProgressOption.checked();
    refresh();
  });
  osdProgressHint.setText("Shows measured achievement progress using the locked badge and progress text only")
    .setFont(Font().setSize(7.0))
    .setForegroundColor(SystemColor::Sublabel);

  osdMessagesLayout.setAlignment(1).setPadding(24_sx, 0);
  osdMessagesOption.setText("Show RetroAchievements message pop-ups").setChecked(settings.achievements.osdMessages).onToggle([&] {
    settings.achievements.osdMessages = osdMessagesOption.checked();
    refresh();
  });
  osdMessagesHint.setText("Shows game-load, warning, connection, completion, and server message toasts")
    .setFont(Font().setSize(7.0))
    .setForegroundColor(SystemColor::Sublabel);

  osdLeaderboardsLayout.setAlignment(1).setPadding(24_sx, 0);
  osdLeaderboardsOption.setText("Show leaderboard pop-ups").setChecked(settings.achievements.osdLeaderboards).onToggle([&] {
    settings.achievements.osdLeaderboards = osdLeaderboardsOption.checked();
    refresh();
  });
  osdLeaderboardsHint.setText("Shows leaderboard start, submit, failure, and result toasts")
    .setFont(Font().setSize(7.0))
    .setForegroundColor(SystemColor::Sublabel);

  osdDurationLayout.setAlignment(1).setPadding(24_sx, 0);
  osdDurationLabel.setText("OSD duration:");
  osdDurationValue.setEditable(true).setText({settings.achievements.osdDurationMs}).onChange([&] {
    settings.achievements.osdDurationMs = clampOsdTiming(osdDurationValue.text(), 5000);
  });

  osdFadeInLayout.setAlignment(1).setPadding(24_sx, 0);
  osdFadeInLabel.setText("OSD fade-in:");
  osdFadeInValue.setEditable(true).setText({settings.achievements.osdFadeInMs}).onChange([&] {
    settings.achievements.osdFadeInMs = clampOsdTiming(osdFadeInValue.text(), 220);
  });

  osdFadeOutLayout.setAlignment(1).setPadding(24_sx, 0);
  osdFadeOutLabel.setText("OSD fade-out:");
  osdFadeOutValue.setEditable(true).setText({settings.achievements.osdFadeOutMs}).onChange([&] {
    settings.achievements.osdFadeOutMs = clampOsdTiming(osdFadeOutValue.text(), 700);
  });

  osdAccentLayout.setAlignment(1).setPadding(24_sx, 0);
  osdAccentLabel.setText("OSD accent:");
  osdAccentValue.onChange([&] {
    if(auto item = osdAccentValue.selected()) settings.achievements.osdAccentColor = item.text();
  });
  for(auto color : {
    "Blue", "Cyan", "Fuchsia", "Green", "Lime", "Orange", "Red", "White", "Yellow"
  }) {
    ComboButtonItem item{&osdAccentValue};
    item.setText(color);
    if(settings.achievements.osdAccentColor == color) item.setSelected();
  }

  osdPositionLayout.setAlignment(1).setPadding(24_sx, 0);
  osdPositionLabel.setText("OSD position:");
  osdPositionValue.onChange([&] {
    if(auto item = osdPositionValue.selected()) settings.achievements.osdPosition = item.text();
  });
  for(auto position : {
    "Top Left", "Top Center", "Top Right",
    "Middle Left", "Middle Center", "Middle Right",
    "Bottom Left", "Bottom Center", "Bottom Right"
  }) {
    ComboButtonItem item{&osdPositionValue};
    item.setText(position);
    if(settings.achievements.osdPosition == position) item.setSelected();
  }

  debugLayout.setAlignment(1).setPadding(12_sx, 0);
  debugOption.setText("Enable RetroAchievements debug logging").setChecked(settings.achievements.debugLogging).onToggle([&] {
    settings.achievements.debugLogging = debugOption.checked();
    refresh();
  });
  debugHint.setText("Prints detailed RetroAchievements runtime events to the terminal")
    .setFont(Font().setSize(7.0))
    .setForegroundColor(SystemColor::Sublabel);

  debugProgressLayout.setAlignment(1).setPadding(24_sx, 0);
  debugProgressOption.setText("Show achievements progress logs").setChecked(settings.achievements.debugProgress).onToggle([&] {
    settings.achievements.debugProgress = debugProgressOption.checked();
    refresh();
  });
  debugProgressHint.setText("Logs progress/challenge indicator events")
    .setFont(Font().setSize(7.0))
    .setForegroundColor(SystemColor::Sublabel);

  debugMemoryLayout.setAlignment(1).setPadding(24_sx, 0);
  debugMemoryOption.setText("Show memory read logs").setChecked(settings.achievements.debugMemory).onToggle([&] {
    settings.achievements.debugMemory = debugMemoryOption.checked();
    refresh();
  });
  debugMemoryHint.setText("Logs per-second memory read snapshots")
    .setFont(Font().setSize(7.0))
    .setForegroundColor(SystemColor::Sublabel);

  refresh();
}

auto AchievementSettings::refresh() -> void {
  auto supported = retroAchievements.supported();

  if(!supported) {
    integrationOption.setChecked(false).setEnabled(false);
    integrationHint.setText("This build was compiled without RetroAchievements support.");
    accountLabel.setVisible(false);
    usernameLayout.setVisible(false);
    passwordLayout.setVisible(false);
    accountButtonsLayout.setVisible(false);
    usernameValue.setEnabled(false);
    passwordValue.setEnabled(false);
    loginButton.setEnabled(false);
    profileTopGap.setVisible(false);
    profileLayout.setVisible(false);
    profileBottomGap.setVisible(false);
    logoutButton.setEnabled(false);
    behaviorLabel.setVisible(false);
    encoreLayout.setVisible(false);
    hardcoreLayout.setVisible(false);
    osdEnableLayout.setVisible(false);
    osdAchievementsLayout.setVisible(false);
    osdProgressLayout.setVisible(false);
    osdMessagesLayout.setVisible(false);
    osdLeaderboardsLayout.setVisible(false);
    osdDurationLayout.setVisible(false);
    osdFadeInLayout.setVisible(false);
    osdFadeOutLayout.setVisible(false);
    osdAccentLayout.setVisible(false);
    osdPositionLayout.setVisible(false);
    debugLayout.setVisible(false);
    debugProgressLayout.setVisible(false);
    debugMemoryLayout.setVisible(false);
    encoreOption.setEnabled(false);
    hardcoreOption.setEnabled(false);
    osdEnableOption.setEnabled(false);
    osdAchievementsOption.setEnabled(false);
    osdProgressOption.setEnabled(false);
    osdMessagesOption.setEnabled(false);
    osdLeaderboardsOption.setEnabled(false);
    osdDurationValue.setEnabled(false);
    osdFadeInValue.setEnabled(false);
    osdFadeOutValue.setEnabled(false);
    osdAccentValue.setEnabled(false);
    osdPositionValue.setEnabled(false);
    debugOption.setEnabled(false);
    debugProgressOption.setEnabled(false);
    debugMemoryOption.setEnabled(false);
    accountStatus.setText("Integration unavailable in this build");
    settingsWindow.panelContainer.resize();
    return;
  }

  auto enabled = settings.general.retroAchievements;
  auto loggedIn = retroAchievements.hasUser();
  if(loggedIn && !wasLoggedIn) passwordValue.setText();
  wasLoggedIn = loggedIn;

  integrationHint.setText("Enable achievements through RetroAchievements.org");
  integrationOption.setChecked(enabled);
  integrationHint.setVisible(enabled);
  accountLabel.setVisible(enabled);
  usernameLayout.setVisible(enabled && !loggedIn);
  passwordLayout.setVisible(enabled && !loggedIn);
  accountButtonsLayout.setVisible(enabled && !loggedIn);
  usernameValue.setEnabled(enabled && !loggedIn);
  passwordValue.setEnabled(enabled && !loggedIn);
  loginButton.setEnabled(enabled && !loggedIn);
  profileTopGap.setVisible(enabled && loggedIn);
  profileLayout.setVisible(enabled && loggedIn);
  profileBottomGap.setVisible(enabled && loggedIn);
  logoutButton.setEnabled(enabled && loggedIn);
  behaviorLabel.setVisible(enabled);
  encoreLayout.setVisible(enabled);
  hardcoreLayout.setVisible(enabled);
  osdEnableLayout.setVisible(enabled);
  osdAchievementsLayout.setVisible(enabled);
  osdProgressLayout.setVisible(enabled);
  osdMessagesLayout.setVisible(enabled);
  osdLeaderboardsLayout.setVisible(enabled);
  osdDurationLayout.setVisible(enabled);
  osdFadeInLayout.setVisible(enabled);
  osdFadeOutLayout.setVisible(enabled);
  osdAccentLayout.setVisible(enabled);
  osdPositionLayout.setVisible(enabled);
  debugLayout.setVisible(enabled);
  debugProgressLayout.setVisible(enabled);
  debugMemoryLayout.setVisible(enabled);
  encoreOption.setChecked(settings.achievements.encore).setEnabled(enabled);
  hardcoreToggleGuard = true;
  hardcoreOption.setChecked(settings.achievements.hardcore).setEnabled(enabled && loggedIn);
  hardcoreToggleGuard = false;
  osdEnableOption.setChecked(settings.achievements.osdEnable).setEnabled(enabled);
  osdAchievementsOption.setChecked(settings.achievements.osdAchievements).setEnabled(enabled && settings.achievements.osdEnable);
  osdProgressOption.setChecked(settings.achievements.osdProgress).setEnabled(enabled && settings.achievements.osdEnable);
  osdMessagesOption.setChecked(settings.achievements.osdMessages).setEnabled(enabled && settings.achievements.osdEnable);
  osdLeaderboardsOption.setChecked(settings.achievements.osdLeaderboards).setEnabled(enabled && settings.achievements.osdEnable);
  osdDurationValue.setEnabled(enabled && settings.achievements.osdEnable);
  osdFadeInValue.setEnabled(enabled && settings.achievements.osdEnable);
  osdFadeOutValue.setEnabled(enabled && settings.achievements.osdEnable);
  osdAccentValue.setEnabled(enabled && settings.achievements.osdEnable);
  osdPositionValue.setEnabled(enabled && settings.achievements.osdEnable);
  debugOption.setChecked(settings.achievements.debugLogging).setEnabled(enabled);
  debugProgressOption.setChecked(settings.achievements.debugProgress).setEnabled(enabled && settings.achievements.debugLogging);
  debugMemoryOption.setChecked(settings.achievements.debugMemory).setEnabled(enabled && settings.achievements.debugLogging);

  if(osdDurationValue.text() != string{settings.achievements.osdDurationMs}) osdDurationValue.setText(string{settings.achievements.osdDurationMs});
  if(osdFadeInValue.text() != string{settings.achievements.osdFadeInMs}) osdFadeInValue.setText(string{settings.achievements.osdFadeInMs});
  if(osdFadeOutValue.text() != string{settings.achievements.osdFadeOutMs}) osdFadeOutValue.setText(string{settings.achievements.osdFadeOutMs});
  if(auto item = osdAccentValue.selected()) {
    if(item.text() != settings.achievements.osdAccentColor) {
      for(u32 index : range(osdAccentValue.itemCount())) {
        if(auto entry = osdAccentValue.item(index); entry && entry.text() == settings.achievements.osdAccentColor) {
          entry.setSelected();
          break;
        }
      }
    }
  }

  if(auto item = osdPositionValue.selected()) {
    if(item.text() != settings.achievements.osdPosition) {
      for(u32 index : range(osdPositionValue.itemCount())) {
        if(auto entry = osdPositionValue.item(index); entry && entry.text() == settings.achievements.osdPosition) {
          entry.setSelected();
          break;
        }
      }
    }
  }

  if(loggedIn) {
    auto user = retroAchievements.username();
    auto score = retroAchievements.userScore();
    profileName.setText(user ? user : settings.achievements.username);
    profilePoints.setText({score, " points"});
#if ARES_ENABLE_RCHEEVOS
    auto avatarUrl = retroAchievements.avatarUrl();
    cachedAvatarUrl = avatarUrl;
    cachedAvatarImage = RA::Images::fetchUserAvatar(user, avatarUrl, 48);
    profileAvatar.setIcon(cachedAvatarImage ? (multiFactorImage)cachedAvatarImage : (multiFactorImage)Icon::Action::Bookmark);
#else
    profileAvatar.setIcon(Icon::Action::Bookmark);
#endif
  } else if(enabled) {
    cachedAvatarUrl = {};
    cachedAvatarImage = {};
    profileAvatar.setIcon();
    accountStatus.setText("Not logged in");
  } else {
    cachedAvatarUrl = {};
    cachedAvatarImage = {};
    profileAvatar.setIcon();
    accountStatus.setText("Integration disabled");
  }

  presentation.refreshToolsMenu();
  settingsWindow.panelContainer.resize();
}

auto AchievementSettings::setVisible(bool visible) -> AchievementSettings& {
  if(visible) refresh();
  return VerticalLayout::setVisible(visible), *this;
}
