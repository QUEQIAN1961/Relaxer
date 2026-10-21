# C6-B Widgets/QML parity matrix

Reference: `v0.2.0` Widgets (`Window`, unchanged business behavior); inspection
started at C6-A checkpoint `dfb9b90`, with final B verification on `d3ae2f2`. PASS describes the specified
existing capability, not a claim of physical user testing or production-data
testing. Native Qt events and service tests are distinguished below.

| Capability | Widgets / shared authority | Actual QML entry and persistence | Evidence / risk | Final state |
|---|---|---|---|---|
| Library / metadata | Window / Library | LocalPage → LibraryBridge / typed TrackListModel | qml_c2c3, library_models, native virtual delegates; owning connection | PARITY PASS |
| Playlists / metadata / ordering | Window / Library playlists | PlaylistsPage, PlaylistEditor, track menus → LibraryBridge; existing tables/resources | qml_ui_parity_library bulk order, tags/description/cover, reopen | PARITY PASS |
| Favorites | Window / Library favorite | Footer and track hearts, sidebar liked scope → LibraryBridge | real UI / model parity tests | PARITY PASS |
| Search / artist / album / sort | SongTable / Library | LocalPage → TrackFilterModel; stable selected ID | library_models + ui_parity_library; 0/1k/10k forced sort mappings and every-row order verification | PARITY PASS |
| Profile / recent / frequent / statistics | Window / sessions + profile settings | ProfilePage → LibraryBridge, existing session/statistics keys | ui_parity_library preserves playback updates without resetting ordinary lists | PARITY PASS |
| Playback / authoritative queue / modes | PlaybackFacade / Queue | PlayerBar, queue/forecast → PlaybackBridge, QueueListModel, UpcomingModel | c2c3, upcoming_parity; one authority, no progress resets | PARITY PASS |
| Seek / volume / mute | PlaybackFacade | PlayerBar / expanded player → PlaybackBridge | real seeks, mute restore and recreation; device listening is separate | PARITY PASS |
| Local lyrics / bilingual / ±5s delay | LyricsDocumentReader / Library offset | LyricsList / tools → LyricsModel | native C6 sync, 1.2 scale, offset boundaries and persistence | PARITY PASS |
| Manual scrolling / fixed timestamp | Existing lyric timeline | LyricsList → current model timestamp / seekRow | native C6 3s reset and midpoint click; reduceMotion exception preserved | PARITY PASS |
| Lyric lookup / saved candidates | LyricsSearchCoordinator / existing cache | Local/multi-select/context entry → LyricsWorkspace + LyricsSearchBridge | bridge/native C6; retained complete/pending results and exit confirmation | PARITY PASS |
| Translation / consent / previews | TranslationCoordinator / AiRuntime | LyricsWorkspace → TranslationBridge; exact consent token | mock positive/negative/save/discard/identity/reopen; 0 real paid calls | PARITY PASS |
| AI configuration / models / credentials | AiRuntime / native credential store | Settings independent AI entry → AiBridge | ai_parity 11 scenarios + actual dropdown; fake credentials in tests | PARITY PASS |
| Browser-provider attention / resume | Existing browser adapter / coordinator | Workspace browser controls → TranslationBridge | coordinator FakeBrowser attention/resume tests; real login/CAPTCHA NOT EXECUTED | PARITY PASS (real login outside automated scope) |
| Expand / fullscreen / cover | Same Window / facade / managed artwork | Footer cover/title, fullscreen button and Esc | native C6 geometry/identity + animation time series | PARITY PASS |
| File / multi-file / folder / drop import | ImportCoordinator / Library | ImportDialog anchored menu + DropArea → ImportBridge | current native matrix and deferred-close tests; user folder-menu PASS | PARITY PASS |
| Preferences / window bounds | SettingsStore | SettingsPage → SettingsBridge; existing keys | settings_parity, Widgets/QML reopen/session test | PARITY PASS |
| Fonts / regions / managed assets | Existing Fonts directory and uiFont schema | FontDialog → SettingsBridge → UiText fontRegion | repaired missing footer/next/translation/metadata bindings; five one-at-a-time region configurations checked on actual QML text; managed file and screenshot | PARITY PASS |
| Theme / appearance authoring / history / images | AppearanceStore | AppearancePage → SettingsBridge preview/save/cancel/rename/delete | settings_parity store tests; native shared-image pixel samples across transition, screenshot; existing bounded image cache | PARITY PASS |
| Tray / hide / restore / quit | QSystemTrayIcon / same UiIntegration + facade | close-to-tray setting; existing tray QActions | actual tray availability and close-to-tray hide/restore; real QAction playback/previous/next/quit; one facade/connection | PARITY PASS |
| App / global shortcuts / recording | PlayerShortcuts validation + existing registration | ShortcutsPage → UiIntegration; portable existing schema | actual app events/editable focus, Windows registration/conflict rollback/release/re-register and WM_HOTKEY OS queue dispatch; OS SendInput NOT EXECUTED without foreground desktop | PARITY PASS (physical-key coverage limit) |
| Context / bulk menus and confirmations | Library / coordinator validators | RoundMenu, selection bar, ConfirmDialog | ui2_refinement + v020_ui_parity + workspace tests | PARITY PASS |
| Version history / errors / exit | Existing changelog / bridges / host lock | VersionLogPage, Toast, status fields, requestQuit | c4c5/native C6 pending search and import exit; error and failure paths tested | PARITY PASS |
| No-local-lyrics replacement popup change | User record-only C6-D01 | Existing popup logic retained | No new authorization to implement; explicitly not marked fixed | INTENTIONALLY DEFERRED |

The deep native-font audit found actual missing region bindings, now repaired
without changing the existing font/store format. No important daily capability
remains MISSING in this matrix. Native tray QActions are programmatically
triggered; Explorer icon mouse input and physical keyboard input are not
claimed. RegisterHotKey is exercised in actual separate OS threads; the isolated
test desktop reports GetForegroundWindow()==0, so SendInput is NOT EXECUTED and
registered native-handler plus OS thread-message dispatch are tested instead.
This is an explicit R16/manual validation limit, not a hidden QSKIP or a claim
that an unavailable physical operation passed. Existing advanced appearance authoring is present;
the older C4+C5 survey's optional-defer wording is superseded by current source.

Performance probe now forces complete proxy mappings and verifies every title
in order. It navigates current settings/AI entries rather than removed tabs.
Fresh native processes at 0/1k/10k tracks measured complete sorting plus mapping
at 0.118/19.3617/36.6969 ms; filter-to-frame at 3.7828/9.6427/42.7031 ms.
10k queue/playback readiness was 227.9063 ms; visible/cached delegates peaked at
15; 5000 lyric rows peaked at 9 delegates and no position-driven resets. These
are same-machine software-renderer response samples, not FPS or cross-hardware
guarantees. No speculative model/core optimization was needed after these checks.

Evidence: `D:/Git/Relaxer/phase-c-c6-work-20261021/Evidence/`
`C6-B-targeted/Run-20261021T072531Z-0520e7f7` (4/4 CTest; native parity 6/6)
and `C6-B-performance/Run-20261021T072725Z-caadd0f8` (all three runs exit 0).
The earlier native test's foreground-window failure is preserved under
`C6-B-native-ctest/Run-20261021T071517Z-b7e12207` and explained above.

Preserved: UI2 layout and 22px content offset, import anchor, left footer
expand, next-panel/backdrop motion, lyric smooth follow independent of reduced
motion, existing stores/schema/core. New providers, sync and core rewrites are
outside this parity batch. C6-A evidence remains at `docs/phase-c/c6-a-gate.md`.
