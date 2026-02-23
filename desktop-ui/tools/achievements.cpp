namespace {

static constexpr u32 kBadgeSize = 32;
static constexpr u32 kHardcoreBorder = 0xffffd54a;

auto clearTableRows(TableView& table) -> void {
  while(table.itemCount()) table.remove(table.item(table.itemCount() - 1));
}

auto configureAchievementsColumns(TableView& table) -> void {
  if(table.columnCount()) return;
  table.setHeadered();
  table.append(TableViewColumn().setText("Badge"));
  table.append(TableViewColumn().setText("Achievement").setExpandable());
  table.append(TableViewColumn().setText("Description").setExpandable());
  table.append(TableViewColumn().setText("Progress"));
  table.append(TableViewColumn().setText("Pts").setAlignment(1.0));
}

#if ARES_ENABLE_RCHEEVOS
auto isWarningAchievement(const rc_client_achievement_t* achievement) -> bool {
  if(!achievement) return false;
  auto title = achievement->title ? string{achievement->title} : string{};
  return achievement->id == 101000001 || (achievement->points == 0 && title.beginsWith("Warning:"));
}
#endif

auto stylizeBadge(image badge, bool locked, bool hardcoreUnlocked) -> image {
  if(!badge) return {};

  auto* pixels = (u32*)badge.data();
  auto count = badge.width() * badge.height();

  if(locked) {
    for(u32 index : range(count)) {
      auto pixel = pixels[index];
      auto alpha = pixel >> 24;
      auto red = (pixel >> 16) & 255;
      auto green = (pixel >> 8) & 255;
      auto blue = pixel & 255;
      auto luma = (red * 54 + green * 183 + blue * 19) / 256;
      pixels[index] = (alpha << 24) | (luma << 16) | (luma << 8) | luma;
    }
  }

  if(hardcoreUnlocked && badge.width() >= 2 && badge.height() >= 2) {
    for(u32 x : range(badge.width())) {
      pixels[x] = kHardcoreBorder;
      pixels[(badge.height() - 1) * badge.width() + x] = kHardcoreBorder;
    }
    for(u32 y : range(badge.height())) {
      pixels[y * badge.width()] = kHardcoreBorder;
      pixels[y * badge.width() + badge.width() - 1] = kHardcoreBorder;
    }
  }

  return badge;
}

#if ARES_ENABLE_RCHEEVOS
auto progressText(const rc_client_achievement_t* achievement) -> string {
  if(!achievement) return {};
  if(achievement->measured_progress[0]) return achievement->measured_progress;
  if(achievement->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE) return "Unlocked (Hardcore)";
  if(achievement->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE) return "Unlocked";
  if(achievement->state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE) return "In progress";
  return "Locked";
}
#endif

auto titleColor(bool unlocked) -> Color {
  return unlocked ? Color{40, 120, 60} : Color{130, 130, 130};
}

auto descriptionColor(bool unlocked) -> Color {
  return unlocked ? Color{70, 70, 70} : Color{145, 145, 145};
}

auto progressColor(bool hardcoreUnlocked, bool unlocked) -> Color {
  return hardcoreUnlocked ? Color{160, 120, 20} : titleColor(unlocked);
}

#if ARES_ENABLE_RCHEEVOS
static constexpr u32 kLeaderboardAvatarSize = 24;
static constexpr u32 kLeaderboardNearbyCount = 25;
static constexpr u32 kLeaderboardAllCount = 100;

auto buildAchievementRows(rc_client_t* client) -> std::vector<AchievementsViewer::Row> {
  std::vector<AchievementsViewer::Row> rows;
  if(!client) return rows;

  auto* list = rc_client_create_achievement_list(
    client,
    RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
    RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS
  );
  if(!list) return rows;

  for(u32 bucketIndex : range(list->num_buckets)) {
    auto& bucket = list->buckets[bucketIndex];
    for(u32 achievementIndex : range(bucket.num_achievements)) {
      auto* achievement = bucket.achievements[achievementIndex];
      if(!achievement || isWarningAchievement(achievement)) continue;

      auto unlockedSoftcore = achievement->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE;
      auto unlockedHardcore = achievement->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE;
      auto unlocked = unlockedSoftcore || unlockedHardcore;

      AchievementsViewer::Row row;
      row.id = achievement->id;
      row.title = achievement->title ? achievement->title : "Untitled";
      row.description = achievement->description ? achievement->description : "";
      row.progress = progressText(achievement);
      row.points = achievement->points;
      row.unlocked = unlocked;
      row.hardcoreUnlocked = unlockedHardcore;
      row.unlockTime = unlocked ? (u64)achievement->unlock_time : 0;
      rows.push_back(std::move(row));
    }
  }

  rc_client_destroy_achievement_list(list);

  std::stable_sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs) {
    if(lhs.unlocked != rhs.unlocked) return lhs.unlocked > rhs.unlocked;
    if(lhs.unlocked && rhs.unlocked && lhs.unlockTime != rhs.unlockTime) return lhs.unlockTime > rhs.unlockTime;
    return lhs.id < rhs.id;
  });

  return rows;
}

auto rowsEqual(const std::vector<AchievementsViewer::Row>& lhs, const std::vector<AchievementsViewer::Row>& rhs) -> bool {
  if(lhs.size() != rhs.size()) return false;
  for(u32 index : range(lhs.size())) {
    auto& l = lhs[index];
    auto& r = rhs[index];
    if(
      l.id != r.id || l.title != r.title || l.description != r.description || l.progress != r.progress ||
      l.points != r.points || l.unlocked != r.unlocked || l.hardcoreUnlocked != r.hardcoreUnlocked || l.unlockTime != r.unlockTime
    ) return false;
  }
  return true;
}

auto buildLeaderboardRows(rc_client_t* client) -> std::vector<LeaderboardsViewer::LeaderboardRow> {
  std::vector<LeaderboardsViewer::LeaderboardRow> rows;
  if(!client) return rows;

  auto* list = rc_client_create_leaderboard_list(client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE);
  if(!list) return rows;

  for(u32 bucketIndex : range(list->num_buckets)) {
    auto& bucket = list->buckets[bucketIndex];
    for(u32 leaderboardIndex : range(bucket.num_leaderboards)) {
      auto* leaderboard = bucket.leaderboards[leaderboardIndex];
      if(!leaderboard) continue;

      LeaderboardsViewer::LeaderboardRow row;
      row.id = leaderboard->id;
      row.title = leaderboard->title ? leaderboard->title : "Untitled";
      row.description = leaderboard->description ? leaderboard->description : "";
      row.trackerValue = leaderboard->tracker_value ? leaderboard->tracker_value : "";
      row.state = leaderboard->state;
      row.format = leaderboard->format;
      row.lowerIsBetter = leaderboard->lower_is_better;
      rows.push_back(std::move(row));
    }
  }

  rc_client_destroy_leaderboard_list(list);

  std::stable_sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs) {
    auto lhsActive = lhs.state == RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
    auto rhsActive = rhs.state == RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
    if(lhsActive != rhsActive) return lhsActive > rhsActive;
    return lhs.title < rhs.title;
  });
  return rows;
}

auto entryRowsEqual(const std::vector<LeaderboardsViewer::EntryRow>& lhs, const std::vector<LeaderboardsViewer::EntryRow>& rhs) -> bool {
  if(lhs.size() != rhs.size()) return false;
  for(u32 index : range(lhs.size())) {
    auto& l = lhs[index];
    auto& r = rhs[index];
    if(l.rank != r.rank || l.user != r.user || l.score != r.score || l.avatarUrl != r.avatarUrl || l.isSelf != r.isSelf) return false;
  }
  return true;
}

struct LeaderboardFetchCallbackData {
  LeaderboardsViewer* viewer = nullptr;
  u64 generation = 0;
};

auto onLeaderboardEntriesFetched(
  int result, const char* errorMessage, rc_client_leaderboard_entry_list_t* list, rc_client_t* client, void* userdata
) -> void {
  std::unique_ptr<LeaderboardFetchCallbackData> callbackData{(LeaderboardFetchCallbackData*)userdata};
  if(!callbackData || !callbackData->viewer) {
    if(list) rc_client_destroy_leaderboard_entry_list(list);
    return;
  }

  auto& viewer = *callbackData->viewer;
  std::vector<LeaderboardsViewer::EntryRow> rows;
  bool dirty = false;

  {
    std::lock_guard lock{viewer.fetchMutex};
    if(callbackData->generation != viewer.fetchGeneration) {
      if(list) rc_client_destroy_leaderboard_entry_list(list);
      return;
    }

    viewer.fetchHandle = nullptr;
    viewer.fetchPending = false;
    viewer.fetchStatus = {};
    viewer.aroundUserMatched = false;

    if(result != RC_OK || !list) {
      viewer.pendingEntryRows.clear();
      viewer.fetchStatus = {"Fetch failed: ", errorMessage ? errorMessage : "unknown error"};
      viewer.pendingEntriesDirty = true;
      viewer.viewDirty = true;
      dirty = true;
      return;
    }

    auto username = retroAchievements.username();
    for(u32 index : range(list->num_entries)) {
      auto& entry = list->entries[index];
      LeaderboardsViewer::EntryRow row;
      row.rank = entry.rank;
      row.user = entry.user ? entry.user : "Unknown";
      row.score = entry.display;
      char url[512] = {};
      if(rc_client_leaderboard_entry_get_user_image_url(&entry, url, sizeof(url)) == RC_OK) row.avatarUrl = url;
      row.isSelf = username && row.user == username;
      rows.push_back(std::move(row));
    }

    viewer.pendingEntryRows = std::move(rows);
    viewer.pendingEntriesDirty = true;
    if(viewer.showingAllEntries) {
      viewer.fetchStatus = {"Showing top ", list->num_entries, " of ", list->total_entries, " entries"};
    } else if(list->user_index >= 0) {
      viewer.aroundUserMatched = true;
      viewer.fetchStatus = {"Showing entries around your rank (", list->num_entries, " of ", list->total_entries, ")"};
    } else {
      viewer.fetchStatus = {"No submitted score found for your account on this leaderboard. Showing top ", list->num_entries, " entries instead."};
    }
    dirty = true;
  }

  if(list) rc_client_destroy_leaderboard_entry_list(list);
  if(dirty) viewer.viewDirty = true;
}

auto updateLeaderboardEntryRow(TableViewItem item, const LeaderboardsViewer::EntryRow& row) -> void {
  if(!item) return;

  auto avatar = RA::Images::fetchUserAvatar(row.user, row.avatarUrl, kLeaderboardAvatarSize);

  auto title = row.isSelf ? Color{40, 120, 60} : Color{};

  if(auto cell = item.cell(0)) cell.setText(row.rank ? string{row.rank} : "-").setForegroundColor(title);
  if(auto cell = item.cell(1)) cell.setIcon(avatar ? (multiFactorImage)avatar : (multiFactorImage)Icon::Action::Bookmark);
  if(auto cell = item.cell(2)) cell.setText(row.user).setForegroundColor(title);
  if(auto cell = item.cell(3)) cell.setText(row.score).setForegroundColor(title);
}
#endif

auto updateAchievementRow(TableViewItem item, const AchievementsViewer::Row& row, void* clientHandle) -> void {
  if(!item) return;

  image badge;
#if ARES_ENABLE_RCHEEVOS
  auto* client = (rc_client_t*)clientHandle;
  if(client) {
    if(auto* achievement = rc_client_get_achievement_info(client, row.id)) {
      badge = stylizeBadge(RA::Images::fetchAchievementBadge(achievement, row.unlocked, kBadgeSize), !row.unlocked, row.hardcoreUnlocked);
    }
  }
#endif

  if(auto cell = item.cell(0)) {
    cell.setIcon(badge ? (multiFactorImage)badge : (multiFactorImage)Icon::Action::Bookmark);
  }
  if(auto cell = item.cell(1)) {
    cell.setText(row.title).setForegroundColor(titleColor(row.unlocked));
  }
  if(auto cell = item.cell(2)) {
    cell.setText(row.description).setForegroundColor(descriptionColor(row.unlocked));
  }
  if(auto cell = item.cell(3)) {
    cell.setText(row.progress).setForegroundColor(progressColor(row.hardcoreUnlocked, row.unlocked));
  }
  if(auto cell = item.cell(4)) {
    cell.setText(row.points).setForegroundColor(titleColor(row.unlocked));
  }
}

}

auto AchievementsViewer::construct() -> void {
  setCollapsible();
  setVisible(false);

  achievementsLabel.setText("RetroAchievements").setFont(Font().setBold());
  configureAchievementsColumns(achievementsList);
}

auto AchievementsViewer::liveRefresh() -> void {
#if ARES_ENABLE_RCHEEVOS
  if(!visible()) return;
  if(!RA::Images::consumeDirty()) return;
  auto* state = retroAchievements.state();
  if(!state || !state->client) return;

  auto latestRows = buildAchievementRows(state->client);
  if(!rowsEqual(rows, latestRows)) {
    refresh();
    return;
  }

  for(u32 index : range(rows.size())) {
    auto rowItem = achievementsList.item(index);
    if(!rowItem) continue;
    updateAchievementRow(rowItem, rows[index], state->client);
  }
#endif
}

auto AchievementsViewer::reload() -> void {
  refresh();
}

auto AchievementsViewer::unload() -> void {
  rows.clear();
  clearTableRows(achievementsList);
}

auto AchievementsViewer::refresh() -> void {
  configureAchievementsColumns(achievementsList);
  clearTableRows(achievementsList);

#if !ARES_ENABLE_RCHEEVOS
  TableViewItem item{&achievementsList};
  item.append(TableViewCell());
  item.append(TableViewCell().setText("RetroAchievements support is unavailable in this build."));
  item.append(TableViewCell());
  item.append(TableViewCell());
  item.append(TableViewCell());
  return;
#else
  auto* state = retroAchievements.state();
  if(!settings.general.retroAchievements || !retroAchievements.hasUser()) {
    TableViewItem item{&achievementsList};
    item.append(TableViewCell());
    item.append(TableViewCell().setText("Log in to RetroAchievements to view game achievements."));
    item.append(TableViewCell());
    item.append(TableViewCell());
    item.append(TableViewCell());
    return;
  }

  if(!emulator || !state || !state->client || !retroAchievements.sessionReady()) {
    TableViewItem item{&achievementsList};
    item.append(TableViewCell());
    item.append(TableViewCell().setText("No RetroAchievements game session is active."));
    item.append(TableViewCell().setText("Load a supported game to populate this list."));
    item.append(TableViewCell());
    item.append(TableViewCell());
    return;
  }

  if(!state->session.hasVisibleAchievements) {
    TableViewItem item{&achievementsList};
    item.append(TableViewCell());
    item.append(TableViewCell().setText("This game version has no achievements."));
    item.append(TableViewCell().setText(
      state->session.hasAchievements || state->session.hasLeaderboards
        ? "RetroAchievements recognized the game, but this specific RA hash/version has no achievements to show."
        : "RetroAchievements recognized the game, but there are no achievements to show."
    ));
    item.append(TableViewCell());
    item.append(TableViewCell());
    return;
  }

  rows = buildAchievementRows(state->client);
  for(auto& row : rows) {
    TableViewItem item{&achievementsList};
    item.append(TableViewCell().setIcon(Icon::Action::Bookmark));
    item.append(TableViewCell());
    item.append(TableViewCell());
    item.append(TableViewCell());
    item.append(TableViewCell());
  }

  for(u32 index : range(rows.size())) {
    auto rowItem = achievementsList.item(index);
    if(!rowItem) continue;
    updateAchievementRow(rowItem, rows[index], state->client);
  }
  achievementsList.resizeColumns();
  achievementsList.column(0).setWidth(40);
#endif
}

auto AchievementsViewer::setVisible(bool visible) -> AchievementsViewer& {
  if(visible) refresh();
  VerticalLayout::setVisible(visible);
  return *this;
}

auto LeaderboardsViewer::construct() -> void {
  setCollapsible();
  setVisible(false);

  leaderboardsLabel.setText("Leaderboards").setFont(Font().setBold());
  leaderboardsStatus.setForegroundColor(SystemColor::Sublabel);
  leaderboardTitle.setFont(Font().setBold());
  leaderboardDescription.setForegroundColor(SystemColor::Sublabel);
  leaderboardMode.setForegroundColor(SystemColor::Sublabel);
  leaderboardFetchStatus.setForegroundColor(SystemColor::Sublabel);
  leaderboardList.onChange([&] { eventLeaderboardChange(); });
  leaderboardEntries.setHeadered();
  leaderboardEntries.append(TableViewColumn().setText("Rank"));
  leaderboardEntries.append(TableViewColumn().setText("User"));
  leaderboardEntries.append(TableViewColumn().setText("Name").setExpandable());
  leaderboardEntries.append(TableViewColumn().setText("Score").setExpandable());
  leaderboardAroundButton.setText("Around Me").onActivate([&] { eventShowAround(); });
  leaderboardAllButton.setText("Top Entries").onActivate([&] { eventShowAll(); });
}

auto LeaderboardsViewer::liveRefresh() -> void {
#if ARES_ENABLE_RCHEEVOS
  if(!visible()) return;

  bool needsRefresh = viewDirty;
  if(RA::Images::consumeDirty()) {
    for(u32 index : range(entryRows.size())) {
      auto item = leaderboardEntries.item(index);
      if(!item) continue;
      updateLeaderboardEntryRow(item, entryRows[index]);
    }
  }

  if(!needsRefresh && !pendingEntriesDirty) return;

  {
    std::lock_guard lock{fetchMutex};
    if(pendingEntriesDirty) {
      entryRows = pendingEntryRows;
      pendingEntriesDirty = false;
      needsRefresh = true;
    }
  }

  if(needsRefresh) {
    viewDirty = false;
    refresh();
  }
#endif
}

auto LeaderboardsViewer::reload() -> void {
  refresh();
}

auto LeaderboardsViewer::unload() -> void {
  leaderboardList.reset();
  while(leaderboardEntries.itemCount()) leaderboardEntries.remove(leaderboardEntries.item(leaderboardEntries.itemCount() - 1));
  leaderboardRows.clear();
  entryRows.clear();
  selectedLeaderboardId = 0;
  showingAllEntries = false;
  aroundUserMatched = false;
  fetchPending = false;
  viewDirty = false;
  fetchStatus = {};
#if ARES_ENABLE_RCHEEVOS
  if(auto* state = retroAchievements.state(); state && state->client && fetchHandle) {
    rc_client_abort_async(state->client, fetchHandle);
  }
  fetchHandle = nullptr;
  fetchGeneration++;
  std::lock_guard lock{fetchMutex};
  pendingEntryRows.clear();
  pendingEntriesDirty = false;
#endif
}

auto LeaderboardsViewer::refresh() -> void {
  leaderboardList.reset();
  while(leaderboardEntries.itemCount()) leaderboardEntries.remove(leaderboardEntries.item(leaderboardEntries.itemCount() - 1));

#if !ARES_ENABLE_RCHEEVOS
  leaderboardsStatus.setText("RetroAchievements support is unavailable in this build.");
  leaderboardTitle.setText();
  leaderboardDescription.setText();
  leaderboardMode.setText();
  leaderboardFetchStatus.setText();
  return;
#else
  auto* state = retroAchievements.state();
  if(!settings.general.retroAchievements || !retroAchievements.hasUser()) {
    leaderboardsStatus.setText("Log in to RetroAchievements to view leaderboards.");
    leaderboardTitle.setText();
    leaderboardDescription.setText();
    leaderboardMode.setText();
    leaderboardFetchStatus.setText();
    return;
  }

  if(!emulator || !state || !state->client || !retroAchievements.sessionReady()) {
    leaderboardsStatus.setText("No RetroAchievements game session is active.");
    leaderboardTitle.setText();
    leaderboardDescription.setText();
    leaderboardMode.setText();
    leaderboardFetchStatus.setText();
    return;
  }

  leaderboardRows = buildLeaderboardRows(state->client);
  if(leaderboardRows.empty()) {
    leaderboardsStatus.setText(
      state->session.hasAchievements || state->session.hasLeaderboards
        ? "This game version has no leaderboards."
        : "This game has no leaderboards."
    );
    leaderboardTitle.setText();
    leaderboardDescription.setText();
    leaderboardMode.setText();
    leaderboardFetchStatus.setText();
    return;
  }

  leaderboardsStatus.setText({
    leaderboardRows.size(),
    leaderboardRows.size() == 1 ? " leaderboard available." : " leaderboards available."
  });

  bool selectedExists = false;
  for(auto& row : leaderboardRows) {
    auto status = row.state == RC_CLIENT_LEADERBOARD_STATE_ACTIVE ? "[Active] " : "";
    auto item = ListViewItem().setText({status, row.title});
    leaderboardList.append(item);
    if(row.id == selectedLeaderboardId) {
      item.setSelected();
      selectedExists = true;
    }
  }

  if(!selectedExists) {
    selectedLeaderboardId = leaderboardRows.front().id;
    if(auto item = leaderboardList.item(0)) item.setSelected();
  }

  auto selectedIndex = 0u;
  for(u32 index : range(leaderboardRows.size())) {
    if(leaderboardRows[index].id == selectedLeaderboardId) {
      selectedIndex = index;
      break;
    }
  }

  auto& selected = leaderboardRows[selectedIndex];
  string modeText = selected.lowerIsBetter ? "Lower is better" : "Higher is better";
  if(selected.trackerValue) modeText.append("  |  Current: ", selected.trackerValue);
  leaderboardTitle.setText(selected.title);
  leaderboardDescription.setText(selected.description ? selected.description : "No description.");
  leaderboardMode.setText(modeText);
  leaderboardFetchStatus.setText(fetchPending ? "Downloading leaderboard data..." : fetchStatus);
  leaderboardAroundButton.setEnabled(!fetchPending);
  leaderboardAllButton.setEnabled(!fetchPending);

  for(auto& row : entryRows) {
    TableViewItem item{&leaderboardEntries};
    item.append(TableViewCell());
    item.append(TableViewCell().setIcon(Icon::Action::Bookmark));
    item.append(TableViewCell());
    item.append(TableViewCell());
  }

  for(u32 index : range(entryRows.size())) {
    if(auto item = leaderboardEntries.item(index)) updateLeaderboardEntryRow(item, entryRows[index]);
  }

  leaderboardEntries.resizeColumns();
  leaderboardEntries.column(0).setWidth(56);
  leaderboardEntries.column(1).setWidth(36);
#endif
}

auto LeaderboardsViewer::eventLeaderboardChange() -> void {
#if ARES_ENABLE_RCHEEVOS
  auto selected = leaderboardList.selected();
  if(!selected) return;
  if(selected.offset() >= leaderboardRows.size()) return;
  selectedLeaderboardId = leaderboardRows[selected.offset()].id;
  eventShowAround();
#endif
}

auto LeaderboardsViewer::eventShowAround() -> void {
#if ARES_ENABLE_RCHEEVOS
  auto* state = retroAchievements.state();
  if(!state || !state->client || !selectedLeaderboardId) return;
  if(fetchHandle) rc_client_abort_async(state->client, fetchHandle);

  {
    std::lock_guard lock{fetchMutex};
    pendingEntryRows.clear();
    pendingEntriesDirty = true;
  }
  entryRows.clear();
  showingAllEntries = false;
  aroundUserMatched = false;
  fetchPending = true;
  fetchStatus = "Downloading leaderboard data...";
  viewDirty = true;
  fetchGeneration++;
  fetchHandle = rc_client_begin_fetch_leaderboard_entries_around_user(
    state->client, selectedLeaderboardId, kLeaderboardNearbyCount, onLeaderboardEntriesFetched, new LeaderboardFetchCallbackData{this, fetchGeneration}
  );
#endif
}

auto LeaderboardsViewer::eventShowAll() -> void {
#if ARES_ENABLE_RCHEEVOS
  auto* state = retroAchievements.state();
  if(!state || !state->client || !selectedLeaderboardId) return;
  if(fetchHandle) rc_client_abort_async(state->client, fetchHandle);

  {
    std::lock_guard lock{fetchMutex};
    pendingEntryRows.clear();
    pendingEntriesDirty = true;
  }
  entryRows.clear();
  showingAllEntries = true;
  fetchPending = true;
  fetchStatus = "Downloading leaderboard data...";
  viewDirty = true;
  fetchGeneration++;
  fetchHandle = rc_client_begin_fetch_leaderboard_entries(
    state->client, selectedLeaderboardId, 1, kLeaderboardAllCount, onLeaderboardEntriesFetched, new LeaderboardFetchCallbackData{this, fetchGeneration}
  );
#endif
}

auto LeaderboardsViewer::setVisible(bool visible) -> LeaderboardsViewer& {
  if(visible) {
    refresh();
    if(selectedLeaderboardId) eventShowAround();
  }
  VerticalLayout::setVisible(visible);
  return *this;
}

auto ChallengesViewer::construct() -> void {
  setCollapsible();
  setVisible(false);

  challengesLabel.setText("Challenges").setFont(Font().setBold());
  challengesView.setEditable(false);
}

auto ChallengesViewer::reload() -> void {
  refresh();
}

auto ChallengesViewer::unload() -> void {
  challengesView.setText();
}

auto ChallengesViewer::refresh() -> void {
  challengesView.setText(
    "hello\n\n"
    "challenges coming soon(TM) though idk if they're publicly accessible?"
  );
}

auto ChallengesViewer::setVisible(bool visible) -> ChallengesViewer& {
  if(visible) refresh();
  VerticalLayout::setVisible(visible);
  return *this;
}
