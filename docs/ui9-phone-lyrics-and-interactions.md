# Phone lyric source and interaction fixes — 2026-11-13

## Changes

- Phone lyrics are a selectable, enabled/disabled, reorderable lyric provider, including manual selection and legacy four-provider order migration. Existing manual bindings retain priority.
- Android forwards player-published timestamped translation alongside original lyrics when available. Combined UTF-8 payload is bounded to 64 KiB and repeated snapshots omit unchanged lyric payloads. Availability still depends on the player publishing complete metadata/extras; no private app data is read.
- Translation supplementation works in both directions. It checks original text and timestamps, preserves word timings and existing translations, and can fill remaining untranslated lines even when most of the document is already translated. Pending callbacks remain guarded by song identity and generation.
- Capsule progress accepts pointer input within 18 logical pixels of its outline (previously 9); visible stroke stays unchanged, and off-path Dock input is excluded.
- Settings show a standard Qt Quick BusyIndicator during initial category incubation. Existing asynchronous category caches remain in use.
- Returning from expanded lyrics starts focus and layout movement together rather than waiting for focus to finish. Expanded text, the original-text morph and compact view crossfade; the compact view does not abruptly appear over the previous view.

## Verification

- Windows full Release build and CTest: 5/5 suites passed, including 34 QML checks. Repaired stale tests for the two-view lyric wrapper, intentionally tighter inactive-word spacing, and asynchronous setting editors. Provider drag test explicitly specifies its starting order.
- Added real loopback provider-chain checks for online translation supplementing phone word lyrics, phone translation supplementing manually selected originals, preservation of existing translation and word timing, and clearing phone content on song change.
- QML tests exercise the widened hit region, off-path rejection, keyboard Slider input, cold-category loading indicator, simultaneous focus/movement, and overlapping crossfade opacity before settling.
- Android: testDebugUnitTest, lintDebug and assembleDebug succeeded; 5 unit tests passed.
- Protocol: 8 canonical vectors and 13 rejection cases passed. Bilingual resource checks passed.
- Packaged ui9 executable ran with isolated profiles: home capture, three-mode motion capture, and settings interaction audit all exited 0, with no QML loading warnings/errors. Settings audit: 60 category clicks at 70 ms intervals, 6 retained caches, 0 click failures, 0 warm-cache misses; maximum cold event-loop gap 77 ms and warm gap 14 ms. These are event-loop measurements, not GPU frame-rate claims.
- Actual captures/logs: artifacts/ui9-home.png, artifacts/ui9-motion/, artifacts/ui9-settings/, artifacts/ui9-runtime-*.log; automated results: artifacts/ui9-tests.log and artifacts/ui9-android.log.

## CI and limits

The reported run 43360306867 compiled Windows successfully but failed on five stale QML assertions/waits. Tests were updated to the implemented interaction contract, not disabled. The new source must pass the remote MSVC Windows, Android and protocol jobs after push.

No phone was attached this round. Cross-source behavior is verified through production parsing/provider paths using isolated loopback TEST data, not claimed as verification of every music player's public lyric payload. Real desktop runtime captures use isolated real-song preview files and do not alter the user's pairing/settings.
