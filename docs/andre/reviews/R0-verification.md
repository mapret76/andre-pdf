# R0 verification: are the fixes of R0-A / R0-B real?

Reviewer: independent, fresh context. Commits checked: `54c0e94` (fixes) and `0ac9e0a` (LF attributes), worktree
`/home/andre/projects/andre-pdf-review`, Debug core build `build-review` (GUI and plugin code read only).

Method: read every hunk of `git show 54c0e94` touching an R0 item; reused the test runs already in the worktree
(`out-UnitTestsSignatureBuilder.txt` 65 passed, `out-UnitTestsMobile.txt` 3 passed, `out-UnitTestsDocumentWriter.txt`
6 passed, `out-UnitTestsTimestamp.txt` 29 passed / 1 skipped); built two throw-away probe programs (a copy of
`tst_signaturebuildertest.cpp` and `tst_mobiletest.cpp` with extra slots, linked against the built core library),
ran them and deleted them again. "Probe" below means one of those runs.

Result: 30 findings claimed fixed. 21 verified, 9 partial, 0 not fixed. The 5 items marked "open" are
described honestly. New findings: 3 medium, 6 low, none high.

## Findings of R0

| Finding | Claimed | Verified? | Evidence (file:line, test) |
| --- | --- | --- | --- |
| A-H1 signing an encrypted PDF crashes / fails | fixed | yes | `pdfdocumentreader.cpp:385-388`: a missing callback gets a stub that refuses, so no `bad_function_call`. `pdfdocumentwriter.cpp:445-452` re-reads the original with `password`. `pdfdocumentsigner.cpp:239,329` pass `openPassword` to the incremental writer and the verification. `signatureplugin.cpp:379-409` asks for the password (3 tries) and `:499-508` uses it. Both sign calls are inside `try/catch` (`:663-672`, `:1072-1080`). Test `encryptedCertification` countersigns an encrypted P=2 file: refused without the password, OK with it, prefix kept, both signatures checked. A user-password-only login is enough to write (`isEncryptionAllowed` = authorised). |
| A-M1 lines on rotated pages | fixed (appearance limited) | partial | Line finder is right now: it maps boxes into the displayed page and back (`pdfsignaturelinefinder.cpp:187-190,294,304`); test `signatureLineFinder` (Rotate 90). The contract flow draws every image into the page on a rotated page (`signatureplugin.cpp:855-858,892`). Gaps: (a) the **pack flow** still makes its visible appearance with `createVisibleSignatureField` (`signatureplugin.cpp:1067`), which ignores `/Rotate` (no `/MK /R`, no matrix, `pdfsignaturestamp.cpp:~229-260`), so the last signature and the QR stamp come out sideways. (b) In the contract flow a rotated last line gives an invisible signature with no stamp or QR, and the only warning is the generic "No signature graphics were created... Continue?" from `onSignDigitally`. Only Rotate 90 is tested. |
| A-M2 SA phrasing | fixed | partial | Attestation lines are now excluded, and "Signature & Date", "For the Purchaser" and "Vir die Koper" are accepted (test `signatureLineLabels`). But the change **adds regressions**, see N1: "Signed on behalf of the Seller" is now rejected, and "Date of signature", "Place of signature" and "Signature date" are now accepted (probe). The note that manual "add a line" is still open is honest. |
| A-M3 pack signs every line by default | fixed | yes | `packsigningdialog.cpp:128-131`: nothing is preselected. `:248-252`: OK refuses until a choice is made. The report lists the signed labels (`signatureplugin.cpp:1013-1020,1107`). GUI test `packSigningDialog` was changed to match (read only, not built here). |
| A-M4 fingerprint unverifiable | fixed | partial | `signatureplugin.cpp:529`: no fingerprint when encrypting. Pack flow passes `QByteArray()` (`:1068`). Still open: when `writeIncrementalUpdate` fails on a document with no signatures yet, `pdfdocumentsigner.cpp:247-250` falls back to a whole `write()`, but the stamp was already built with the hash of the original. The fingerprint is then printed but cannot be checked. Same in mobile (`pdfmobilecontroller.cpp:292`). |
| A-M5 other parties cannot sign | fixed (Reader gate open) | yes | `pdfsignaturelinefinder.cpp:77-112` adds empty `/FT /Sig` fields with `/Rect` in page space. Used in the contract flow (`signatureplugin.cpp:874-881`) and the pack flow (`:1044-1051`). Test `contractPack`: the field is found, is not added twice, and `hasExistingSignatures` stays false. Duplicate names are possible, see N5. The Acrobat Reader check is honestly marked open. |
| A-L1 `isOK()` ignored appended bytes | fixed | partial | `isCoveringWholeFile` is added to `isOK()` (`pdfdocumentsigner.h:173`, `.cpp:376`); test with `\n%appended` and with a countersignature. The second half of L1 is not done: `certification` is still read from the catalog `/Perms` (`pdfdocumentsigner.cpp:361`) without checking that `/Perms /DocMDP` points to the field being checked. |
| A-L2 plain library accepted on Windows | fixed | yes | `pdfsignaturelibrary.cpp:191-198`: PLAIN1 is refused on Windows. Side effect: see N6. |
| A-L3 passwords not wiped | open | yes (honest) | No change. |
| A-L4 contract flow half done on cancel | fixed | yes | `signatureplugin.cpp:847` keeps a copy of the document before the flow. `:904-917` restores it when `onSignDigitally` returns false (that covers cancel, error, and cancel at the save dialog). `:511-518` asks before "No changes" when lines are left to others. |
| A-L5 foreign fields in extracted part | fixed | yes | `pdfcontractpack.cpp:252-316`: a field is kept only when a widget is on a kept page (by `/P` or `/Annots`). Test `contractPack`: 1 field in Sale, 0 in Lease. |
| A-L6 bidi controls in file names | fixed | yes | `pdfcontractpack.cpp:321-333`: `\p{Cf}` and DEL removed, base name sanitised, trailing dots dropped; the U+202E test passes. |
| A-L7 QR ECC / CN / throw | fixed | yes | `pdfsignaturestamp.cpp:57` removes control and format characters. `:149-159` uses ECC MEDIUM and catches `data_too_long` (a `std::exception`). The empty-module path draws only the quiet zone (`:182-207`, no division). |
| A-L8 pack check on memory bytes | fixed | yes | `signatureplugin.cpp:1103-1106`: the file is read back from disk. |
| B-H1 mobile aborts on encrypted PDF | fixed | yes | Reader guard as in A-H1. `pdfmobilecontroller.cpp:78-127` asks for the password, passes it to the signer and the check, and `openSigned()` reopens. Probe (encrypted P=2 file): no password -> `false`, `needsPassword=1`; wrong password -> "The password is wrong."; right password -> opens, signs, checks, `Encrypted_SIGNED.pdf`, reopens. There is still no encrypted case in `tst_mobiletest.cpp`. |
| B-H2 OS/2 100 bytes | fixed | yes | `pdfscanfontbuilder.cpp:733-737`: 8 sub/superscript fields, strikeout size and position, family class. Test `scanFont` parses the table: 96 bytes, char range, typo/win metrics, xHeight, ascent 20-60 px. |
| B-H3 Excel numbers | fixed | partial | Test `officeExport` covers every case R0 reported. The "leading zero / 11+ digits stay text" rule is applied only to plain digits (`pdfofficeexporter.cpp:252-258`). Space-grouped values bypass it, see N7. |
| B-M1 font installer trusts catalogue | fixed | yes | `pdffontinstaller.cpp:84-99`: HTTPS only, two hosts pinned, same-origin redirects. `:102-113`: size cap with abort. `:132-136`: id regex. `:350`: file name from the id. `:367`: registry name from the safe name. `:259-264`: trial cache name. Test `fontInstaller`: an id with a path, a `file://` URL and a forged id are all refused. |
| B-M2 foreign OCR text kept | fixed | partial | `pdfscantexteditor.cpp:153-301` removes invisible text on pure scans. But it removes **whole BT/ET objects**, so one-text-object-per-page OCR (Tesseract style) loses the whole page's text layer while the message says only "at this place". Proven, see N2. Test `scanTextForeignOcr` uses one BT per line, so it does not catch this. |
| B-M3 plugin migration | fixed | yes | `pdfprogramcontroller.cpp:2413-2414,3111-3128`: without `KnownPlugins`, only `ScanText` counts as new. That matches the tree: ScanText is the only plugin that is not in upstream `905cefa`, and its JSON `Name` is "ScanText". |
| B-M4 Share/Word use stale file | fixed | yes | `askForSaveBeforeHandOver` (`pdfprogramcontroller.cpp:1634-1652`) is called in E-Mail (`:1548`), Share (`:1802`) and Open in Word (`:1847`). A cancelled Save As returns false. |
| B-M5 U+FFFE/FFFF in XML | fixed | yes | `pdfofficeexporter.cpp:52-72` keeps only XML 1.0 characters, including valid surrogate pairs. The test page carries U+FFFE + lone U+D800, and python `minidom` validates the files. Every user-text `.arg` uses `escape`. |
| B-M6 redline memory | fixed | partial | `pdftextredline.cpp:80-95,121` keeps only diagonals `-d-1..d+1` per round. The backtrack indices stay inside them (k±1 in [-d-1, d+1]). Cap 4000 makes the worst case about 16 M ints, roughly 64 MB instead of 290 MB. Test with 3000 vs 3000 words: under 30 s. Not addressed and not listed as open: the diff still runs on the GUI thread with no cancel (second half of M6). Lowering the cap to 4000 also means texts with 4000+ edits are now reported as "replaced as a whole". |
| B-M7 font name parsing | fixed | yes | `pdffontidentifier.cpp:185-241`: aliases are looked up first, then suffixes are stripped one at a time. Test `fontNames`: `TimesNewRoman,Bold`, `TimesNewRoman`, `ArialBold`, `Heavyweight`. |
| B-L1 download size | fixed | yes | 4 MB JSON / 20 MB TTF caps (`pdffontinstaller.cpp:172,218,239`). |
| B-L2 CDN without integrity | fixed | partial | Hosts are pinned. The note that jsDelivr sees the user's IP was not added (`THIRD-PARTY-NOTICES.md:64` names jsDelivr but not the IP disclosure). |
| B-L3 trial cache name | fixed | yes | `getSafeName` before the path is built (`pdffontinstaller.cpp:259-264`). |
| B-L4 zip64 | open | yes (honest) | No change. |
| B-L5 multi-column interleave | open | yes (honest) | No change. |
| B-L6 mobile overwrote `_SIGNED` | fixed | yes | Numbered names (`pdfmobilecontroller.cpp:194-211`); `Main.qml` reopens the signed file. A content URI still gives a provider-id file name (not claimed). |
| B-L7 .pfx password kept in field | fixed | yes | `Main.qml` clears `passwordField` after each sign attempt; the document password field is cleared on accept and on reject. |
| B-L8 no "open with" intent | open | yes (honest) | No change. |
| B-L9 per-word bitmap | open | yes (honest) | `pdfscanfontbuilder.cpp:66` unchanged. |
| B-L10 partial font install | fixed | partial | `install()` now returns **true** with "%1 styles of %2 were installed, then..." in `errorMessage` (`pdffontinstaller.cpp:338-340`). The only caller (`scantextdialog.cpp:338-344`) shows `errorMessage` only on false, so the user never sees which styles failed. |
| B-L11 export name of unsaved doc | fixed | yes | `pdfprogramcontroller.cpp:1654-1662`. |

## New findings

### N1 medium: line-label rules now reject "Signed on behalf of" and accept date/place-of-signature lines
- `Pdf4QtLibCore/sources/pdfsignaturelinefinder.cpp:56` (`attestation` matches `sign(ed)? on`), `:58` and `:70-73` (`explicitSignature` is checked before `other`).
- Probe output of `isSignatureLabel`: "Signed on behalf of the Seller" -> 0 (it was 1 before the fix); "Date of signature" -> 1, "Place of signature" -> 1, "Signature date" -> 1 (all were 0 before); "Initials and Surname" / "Surname and initials" -> 1 (already wrong before).
- Scenario: an SA agreement with "Signed on behalf of the Seller: ______". The line is missing from the contract checklist, and "every line decided" cannot cover it. In the pack flow with "Sign every signature line", Andre's signature image goes on the "Date of signature: ______" line.
- Fix: exclude only real attestation context (`signed at`, `signed (on )?this`, `day of`, `dag van`). Do not exclude `signed on behalf`, `signed for`, or `namens`. Let `other` (date, place, name, surname) win unless the label pairs both, like "Signature & Date" or "Name and Signature". Add `surname`/`van` to `other`. Add the probe strings to `signatureLineLabels`.

### N2 medium: editing one word on another program's OCR scan wipes the page's whole text layer
- `Pdf4QtLibCore/sources/pdfscantexteditor.cpp:186-221`: the unit of removal is a whole `PDFEditedPageContentElementText`, which is a whole BT...ET. The box is the union of all its items, so if any item touches the area, the whole element is removed.
- Probe: a scan with a single BT block holding three lines, in Tesseract-renderer style. Before the edit, the page text is "Purchase price R 950 000 Elsewhere on the page Third line far away". After editing the first line: `hidden=ForeignRemoved`, and the page text is only "Purchase price R 875 000".
- Scenario: Andre fixes an amount on a scan from NAPS2/ocrmypdf/Tesseract. The rest of the page is no longer searchable or exportable, while the dialog says the OCR text "at this place" was removed.
- Fix: remove only the text-showing items whose own estimated box meets the area, and keep the other items with their state changes (or split the element per item). Otherwise, when the removed element reaches beyond the area, report it ("the hidden text of the whole page was removed, run OCR again"). Add a single-BT case to `scanTextForeignOcr`.

### N3 medium: an approval signature on a document certified "No changes" (P=1) is not refused
- `Pdf4QtLibCore/sources/pdfdocumentsigner.cpp:80-92` checks certification only for the new signature. No caller checks the existing DocMDP P or the encryption permission bits (`signatureplugin.cpp`, `pdfmobilecontroller.cpp:295`). The A-H1 fix makes this newly reachable for Andre's encrypted "No changes" contracts, which used to crash.
- Probe: encrypt + certify P=1, so the owner password is discarded and `ModifyFormFields` is denied. Reopening with the user password and countersigning gives `Result::OK`, and `checkSignedFile(...).isOK() == 1`. PDF4QT's verifier still calls the certification valid, with only the warning "5617 bytes are not covered". Per ISO 32000-2 12.8.2.2, an approval signature after P=1 invalidates the certification, and Acrobat shows it as changed.
- `tst_mobiletest.cpp` even expects this ("signed, so only an approval signature (tap) is possible" after certifying with `1`).
- Fix: in `sign()`, if `getCertificationPermissions(document) == NoChanges` and the new signature is not a `DocTimeStamp`, return a new `Result::DocumentLockedNoChanges`. Optionally also refuse when the file is encrypted without `ModifyFormFields`. Show the reason in the plugin and the mobile app, and change the mobile test.

### N4 low: pack-flow visible signature is sideways on rotated pages (rest of A-M1)
- `signatureplugin.cpp:1064-1068` with `pdfsignaturestamp.cpp` `createVisibleSignatureField`, which has no `/Rotate` handling. Fix: reuse the contract-flow rule (draw into the page, invisible signature), or rotate the form `/Matrix` and set `/MK /R`.

### N5 low: other-party fields get duplicate names on a second round
- `pdfsignaturelinefinder.cpp:91` always names them `<prefix>1..n`. Probe: two rounds on the same file give two root fields both named "Signature of other party 1". Same-name root fields are the same field per the spec, and readers merge or confuse them.
- Fix: count up past the names already in the form.

### N6 low: on Windows a failed DPAPI save writes a library that can no longer be read
- `pdfsignaturelibrary.cpp:186` falls back to PLAIN1 when `CryptProtectData` fails, but `:191` now refuses PLAIN1 on Windows. The save "succeeds" and the signatures are gone at the next start.
- Fix: on Windows, return an error from `protect` and refuse the save.

### N7 low: Excel export still turns space-grouped codes into numbers
- `pdfofficeexporter.cpp:243,260-262`. Probe: "012 345" -> 12345, "62 123 456 789" -> 62123456789, "1 234 567 890 123" -> number, "0123.45" -> 123.45.
- Fix: after removing the separators, apply the leading-zero and more-than-10-digit rules to every branch.

### N8 low: mobile `sign()` is not exception-safe
- `pdfmobilecontroller.cpp:295` calls `PDFDocumentSigner::sign` from a QML `Q_INVOKABLE` without the `try/catch` the desktop plugin got. A `PDFException` from a malformed document aborts the app.
- Fix: wrap it as in `signatureplugin.cpp:663-672`.

### N9 low: pack report built with chained `.arg()` on text from the PDF
- `signatureplugin.cpp:1107-1111` and `:1021`. A bookmark title or label containing "%4" or "%5" gets the later arguments substituted into it, so the report on which lines were signed can be garbled by the document.
- Fix: use the multi-argument `arg(QString, QString, ...)` form, with the numbers converted first.

Not an R0 item (B5 updater, read only): manifest signature, version/rollback check, host pinning, size cap, SHA-256 check,
our own file name and `msiexec` from `%SystemRoot%` all look sound. Low notes, no change required:
- The daily check at start is on by default (`pdfeditormainwindow.cpp:625`), so the first contact with github.com happens before any consent.
- The manifest has no expiry, so an attacker can freeze updates by replaying an older signed manifest.
- The installer is hashed, then started after a user prompt (a time-of-check/time-of-use window against same-user malware only).
