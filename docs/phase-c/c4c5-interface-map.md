# C4+C5 interface survey

Starting point: R14 PASS, f2242c9523ea0200d43ad6fbcba6ab6978dbe364.
This is an internal implementation map; final handoff remains five reports.

| Existing Widgets feature | Existing authority | QML adapter / presentation |
|---|---|---|
| Queue metadata | Library / PlaybackFacade.queue | QueueListModel; selective tracksByIds on the owning connection |
| Local lyrics / timeline | LyricsDocumentReader, Lyrics::parse/indexAt | LyricsModel; no QML parser or independent cache |
| Paired bilingual text | Existing LyricView pairing semantics / ai::lyrics::merge | Read-only row original/translation roles; stable document row |
| Per-song delay | Library::lyricOffset/setLyricOffset | LyricsModel; milliseconds, UI ±5s in 10ms increments |
| Online lyric lookup / cache | LyricsSearchCoordinator / Library lookup cache | Explicit search command and candidate model; never automatic cloud lookup |
| Translation / previews | One TranslationCoordinator, prepare/start/save/discard | TranslationBridge; exact task and track identity; explicit consent |
| AI configuration / key | ai::AiRuntime / WindowsCredentialStore | AiBridge; configuration without exposing a secret property; explicit credential operations |
| Browser translation / login | TranslationBrowserAdapter / existing BrowserTranslation | Same Coordinator path and attention/resume state; no new login automation |
| Theme / base preferences | SettingsStore / AppearanceStore | SettingsBridge; existing keys, validated draft save/cancel |
| Import / cancellation | ImportCoordinator | ImportBridge; task identity, progress, original cancellation/barrier/shutdown |
| Artwork | Track.cover / Library.absolute / existing managed resource | Asynchronous QML Image with bounded sourceSize and fallback |
| Expanded player / fullscreen | Existing QML ApplicationWindow / PlaybackFacade | Same window, geometry restore, Esc; no new player/window |

Settings confirmed in Window::buildSettings: theme, threshold, closeToTray,
lyricColor, uiFont, playerCoverBackground, shortcuts. Translation display uses
showLyricTranslation. AppearanceStore owns appearanceHistory; no replacement
format is introduced. Advanced authoring can remain an explicitly documented
Widgets capability, while the existing theme/history is read compatibly.

Lyrics is the actual local parser/timeline class; there is no separate
LyricsEngine/LyricsDocument class to invent. Core services remain unchanged
except the minimal read-only Library batch API for D-R14-01.

Network and paid operations never run on page construction, navigation,
track change or shell rebuild. All development uses isolated fixtures and
Mock/Fake transports/credentials. A key is never returned to QML or logged.
