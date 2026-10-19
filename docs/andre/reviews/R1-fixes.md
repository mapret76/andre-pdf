# R1 — what was done with the findings of the R0 verification and of the B4/B5 review

Reviews: [R0-verification.md](R0-verification.md) (the R0 fixes, 21 of 30 verified, 9 partly)
and [B45-review.md](B45-review.md) (version history and updater), both on commit 0ac9e0a by
reviewers with a fresh context. The fixes are in the commit that adds this file. They are
reviewed again, together with B1–B3, in R2.

## R0 verification

| Finding | Severity | Status | What changed |
| --- | --- | --- | --- |
| N1 label rules reject "Signed on behalf of", accept date/place lines | medium | fixed | Attestation only for "signed at / this / on <date>", "day of", "dag van"; a signature word next to a name/date/place word counts only when paired ("Signature & Date", "Name and Signature"); "Initials and Surname" is a name line; tests with every probe string. |
| N2 one edited word wipes another program's whole OCR layer | medium | fixed | Only the invisible text items in the area are removed; the other items of the same text object stay (absolute text positions). Test: a single text object with three lines, the other two lines stay. |
| N3 approval signature after "No changes" not refused | medium | fixed | `PDFDocumentSigner::sign` refuses every signature but a document timestamp on a document certified P=1 (`Result::DocumentLockedNoChanges`); desktop and mobile show the reason; tests (builder, mobile). |
| A-M4 fingerprint printed after a rewrite | medium (rest) | fixed | `Parameters::requireIncrementalUpdate`; desktop and mobile require the update when the stamp has the file fingerprint, and sign again without the fingerprint when the update cannot be appended. |
| N4 pack-flow signature sideways on rotated pages | low | fixed | Same rule as the contract flow: on a rotated last page the image goes into the page and the digital signature is invisible. |
| N5 duplicate other-party field names | low | fixed | Names already in the form are skipped. |
| N6 failed DPAPI save writes an unreadable library | low | fixed | On Windows `protect` returns nothing when DPAPI fails, and the save is refused with a message. |
| N7 space-grouped codes become numbers | low | fixed | Leading-zero and more-than-10-digit rules apply to every accepted form; tests. |
| N8 mobile signing not exception-safe | low | fixed | try/catch as on the desktop. |
| N9 pack report garbled by "%4" in a label | low | fixed | One multi-argument `arg()`. |
| B-L2 jsDelivr sees the IP | low (rest) | fixed | Said in the install question and in THIRD-PARTY-NOTICES.md. |
| B-L10 failed styles not shown | low (rest) | fixed | A partial install shows which styles failed. |
| A-M2 manual "add a line" | low | open | Lines are found automatically; adding one by hand is not built. |
| A-L1 certification not tied to the checked field | low | open | `SignedFileCheck::certification` is the document's; the plugin checks the field it just signed. |
| B-H3 rest | low | fixed | With N7. |
| B-M6 redline blocks the window | low | open | Memory is bounded; it runs in the GUI thread. |
| A-L3, B-L4, B-L5, B-L8, B-L9 | low | open | As in R0-fixes.md. |

## B4/B5 review

| Finding | Severity | Status | What changed |
| --- | --- | --- | --- |
| H1 release script finds the artifact directory, never the MSI | high | fixed | `find -type f -name "*_<version>.msi"`, exactly one match required; dry run with a stub `gh` reaches the confirmation and `gh release create`. |
| M1 changelog text rendered as Markdown with HTML | medium | fixed | `toMarkdown` escapes every ASCII punctuation character and line breaks; both dialogs render with `MarkdownNoHTML`; the update dialog follows no links; test with `<Enter>`, `<!--`, a fake heading. |
| M2 installer can be swapped between check and msiexec | medium | mitigated | A new random file name per download (`NewOnly`), earlier installers removed; before msiexec the file is opened without write/delete sharing, hashed again and kept open until the program exits. Remaining: msiexec reads the package again after the program quits; an Authenticode-signed MSI (code signing certificate) closes it. |
| M3 download cannot be cancelled, no overall deadline | medium | fixed | Cancel button (atomic flag, also checked per chunk); deadline 2 min + size at 64 kB/s. Test: a cancelled download leaves no file. |
| M4 release script signs any branch, any version | medium | fixed | Run must be of master, commit on `origin/master` (fetched), version newer than the latest release, manifest tool taken from the commit, summary shown and "yes" required. |
| L1 redirects not held to the allow-list | low | fixed | `UserVerifiedRedirectPolicy`: only HTTPS redirects to the allowed hosts. |
| L2 no freshness, two "latest" requests | low | open | The signature and the version check hold; a date field and one resolved tag are left for later. |
| L3 loose fields | low | fixed | Anchored version pattern, integer size, exactly 64 hex digits, no user info or port; tests. |
| L4 Enter twice installs | low | fixed | Automatic offers default to "Remind Me Later"; the confirmation defaults to No. |
| L5 quit asks twice / installer while open | low | fixed | The window closes first (one save question); only then the installer starts. |
| L6 once-a-day logic | low | fixed | Date stored after a completed check, compared with `==`. |
| L7/L8 check threads | low | fixed | One check at a time; a thread is only deleted after it finished. |
| L9 release script details | low | fixed | Exact notes heading, key mode 600 checked, manifest tool from the commit, draft clean-up documented. Key rotation: verify against the previous key is left for a rotation. |
| L10 old MSIs never removed | low | fixed | Removed before each download. |
| L11 parsers disagree | low | fixed | The tool refuses an invalid version heading; the program attaches nothing to one; test. |
| L12 other platforms | low | fixed (Linux) | No automatic check where no installer is offered; Android has no updater (the app store or a new APK). |
| L13 tests / warning | low | partly | Warning fixed; the dialogs have no tests (GUI). |
| L14 any 256-bit EC key; test hook exported | low | fixed / accepted | Only the curve prime256v1; the test hook stays (code in the process can do anything anyway). |
