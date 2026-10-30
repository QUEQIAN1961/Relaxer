# V0.2.2-RC6 private test candidate

Fixed baseline: c03a926ee1b7883ca1ffe0c4e33d6027f61a6adf (RC5).
This is a new product-fix candidate after RC5, requiring independent acceptance.

- Root Relaxer.exe directly starts the portable application (QML by default;
  --widgets selects compatibility UI). It verifies PORTABLE-SHA256SUMS.txt and
  the compiled source identity of the runtime EXE/QML module. This is integrity
  checking within the delivered ZIP, not an Authenticode signature.
- RelaxerStart.exe remains the source preparation launcher. It retains its
  ready-marker/preparation rules and explains the separate portable entry.
  Starting CMD or the portable app does not complete source preparation.
- Feedback history has a close action, ten-second expiry, outside-click
  dismissal and details close/Escape handling. History remains in session memory.
- The extra full-screen button is removed from the expanded lyrics page.
- Explicit instrumental placeholder documents use exactly 纯音乐，请欣赏,
  including the legacy long sentence and its DJ variant. Ordinary lyrics and
  mixed documents are preserved; genre/title alone cannot mark a song instrumental.
- Version-log navigation resets expansion. The UI groups 0.2.0 and 0.2.2 RC
  entries; raw resource entries, source history and 0.2.0-GA remain intact.
  RC5's resource has no standalone RC1 row; RC2 already records RC1 inheritance.

ART-01 remains UNRESOLVED — NOT REPRODUCED, with no established root cause or fix.
No GA, public release, system update, paid service call, push, merge or tag is
authorized by this candidate. The seven GA gates remain independently open.

Validation logs and package/source identity are supplied in the external
candidate delivery directory. Development failures and subsequent passing runs
are retained separately. Native mouse checks cover only the interactions
actually observed; they do not close the complete native-input acceptance gate.
