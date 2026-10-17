# Phase B7 frozen contracts

Baseline: `4ede1c3456d4edfbb054c06d2d1664be7ff30c63`, tree
`6253a1ac54ec83d49fb55571e532ad83516c8176`. Product remains 1.10.0.

## Observed call chains

`LyricsLookup::translateSelected` reads a saved document, prepares numbered source
lines, validates AiRuntime configuration/context, opens explicit cloud consent,
rechecks selection/file/configuration, then invokes AiRequestCoordinator::complete.
AiRuntime owns the provider registry, credentials and request coordinator. The
request coordinator alone dispatches, times out, cancels and isolates late provider
responses. WholeSongLyricsTranslation validates complete IDs and merges against
local original bytes. LyricsLookup currently owns the source/configuration SHA-256,
generation, pending request ID, paid preview and final preserving-text Library save.
Different LyricsLookup windows can submit independent requests through one runtime.

BrowserTranslation is a retained Core/Network CDP **line** backend with a dedicated
profile/process, token/epoch, cache, 90-second attention timeout and manual resume.
Its only caller is the opt-in online smoke executable. B5.1 removed its normal
Widgets entry point, queue and automatic save. There is no browser whole-song UI
queue in this baseline. B7 must not restore removed UI or add website automation.
Provide a service-level compatibility adapter and controlled whole-song tests.

## External behavior to retain

- One explicit cloud consent -> one request; no retry, splitting or fallback.
- Same model/endpoint/credential checks, timeout budget, payload and usage messages.
- Strict complete response validation; original timestamps/text from local source.
- Completed results require explicit save. Save failure keeps a retryable preview.
- Source identity/path/bytes and configuration changes prevent stale saving.
- Explicit song navigation, multi-select, search or successful lyric replacement
  clears the old result; failed replacement keeps it. UI rows are not task IDs.
- Filtering, refresh and window reactivation preserve in-flight/unsaved results.
- Pending close asks to continue or stop locally. A completed preview does not
  trigger that pending-request dialog; closing destroys the window-owned preview.
  This is the observed existing contract, not persistent paid-result storage.
- Local cancellation cannot revoke remote billing. Cancel before completion wins;
  completion before a request cancellation preserves the valid preview. Explicit
  discard is a separate operation (navigation/replacement/confirmed resend/close).
- Library and SQLite stay on their owning thread; checked save reuses existing
  transaction, SHA-256 recheck and file compensation, never a second disk writer.
- R5.1 assessment cache, audit thread pool and trigger scope remain unchanged.

## Authority after extraction

| Authority | Owner |
|---|---|
| HTTP, credentials, provider/model checks, request timeout and provider IDs | Existing AI infrastructure |
| Strict source preparation / response validation / merge | Existing whole-song functions |
| Song snapshot, consent-ready task, business queue, result ownership, save lifecycle | TranslationCoordinator |
| Browser process/CDP/website protocol/attention timeout | Existing BrowserTranslation |
| Browser segment tokens, queue and complete-song assembly | TranslationCoordinator with backend adapter |
| Row mapping, modal consent, explicit navigation/discard, display/cache refresh | Widgets |
| Files, SQL transaction, compensation | Existing Library checked save |

No B8/B9/QML, schema, playback, version or public changelog changes.
