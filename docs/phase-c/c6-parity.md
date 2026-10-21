# C6-B Widgets/QML parity matrix

Reference: `v0.2.0` Widgets (`Window`, unchanged business behavior); current
QML inspection starts at C6-A checkpoint `dfb9b90`. PASS describes the specified
existing capability, not a claim of physical user testing or production-data
testing. Native Qt events and service tests are distinguished below.

| Capability | Widgets / shared authority | Actual QML entry and persistence | Existing evidence / risk | Initial state |
|---|---|---|---|---|
| Library / metadata | Window / Library | LocalPage → LibraryBridge / typed TrackListModel | qml_c2c3, library_models, native virtual delegates; owning connection | PARITY PASS |
| Playlists / metadata / ordering | Window / Library playlists | PlaylistsPage, PlaylistEditor, track menus → LibraryBridge; existing tables/resources | qml_ui_parity_library bulk order, tags/description/cover, reopen | PARITY PASS |
| Favorites | Window / Library favorite | Footer and track hearts, sidebar liked scope → LibraryBridge | real UI / model parity tests | PARITY PASS |
| Search / artist / album / sort | SongTable / Library | LocalPage → TrackFilterModel; stable selected ID | library_models + ui_parity_library; complete-sort measurement pending | PARITY PASS (measurement pending) |
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
| Fonts / regions / managed assets | Existing Fonts directory and uiFont schema | FontDialog → SettingsBridge | service copy/validation tests; actual per-region rendering pending | PARTIAL (native evidence) |
| Theme / appearance authoring / history / images | AppearanceStore | AppearancePage → SettingsBridge preview/save/cancel/rename/delete | settings_parity native store tests; shared-image render/lifecycle pending | PARTIAL (native evidence) |
| Tray / hide / restore / quit | QSystemTrayIcon / same UiIntegration + facade | close-to-tray setting; existing tray QActions | implementation connected; actual native action/restore evidence pending | PARTIAL (native evidence) |
| App / global shortcuts / recording | PlayerShortcuts validation + existing registration | ShortcutsPage → UiIntegration; portable existing schema | app editable-focus test and validation pass; Windows register/dispatch/release pending | PARTIAL (native evidence) |
| Context / bulk menus and confirmations | Library / coordinator validators | RoundMenu, selection bar, ConfirmDialog | ui2_refinement + v020_ui_parity + workspace tests | PARITY PASS |
| Version history / errors / exit | Existing changelog / bridges / host lock | VersionLogPage, Toast, status fields, requestQuit | c4c5/native C6 pending search and import exit; error and failure paths tested | PARITY PASS |
| No-local-lyrics replacement popup change | User record-only C6-D01 | Existing popup logic retained | No new authorization to implement; explicitly not marked fixed | INTENTIONALLY DEFERRED |

No MISSING daily capability was found by this inspection. PARTIAL entries above
are evidence gaps to close before the B checkpoint; this is not yet a claim of
completed C6-B acceptance. Existing advanced appearance authoring is present;
the older C4+C5 survey's optional-defer wording is superseded by current source.

Performance audit must force complete proxy mapping and verify actual order;
an API call or first-frame latency alone is not a completed sort or FPS.
The old C4+C5 performance probe also refers to removed settings tabs and must
be updated to navigate the actual current QML entries before use.

Preserved: UI2 layout and 22px content offset, import anchor, left footer
expand, next-panel/backdrop motion, lyric smooth follow independent of reduced
motion, existing stores/schema/core. New providers, sync and core rewrites are
outside this parity batch. C6-A evidence remains at `docs/phase-c/c6-a-gate.md`.
