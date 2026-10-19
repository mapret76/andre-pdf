# R0 review B - Andre PDF tasks A9-A15 and the mobile app

Reviewer: independent (fresh context), commit `0ac8a12`, worktree `/home/andre/projects/andre-pdf-review` (Debug build in `build-review`).
Scope: size reduction, Word/Excel export, redline, font identification/installation, scan text editing, scan font builder, the GUI wiring (controller, share sheet, about dialog, notices), the Scan Text plugin, the Compare app redline, `Pdf4QtMobile`.
GUI and Windows-only code was reviewed by reading only (not built here). Findings marked **proven** were reproduced with small probe programs linked against the built core library; the probe sources were deleted afterwards as instructed. Timings below come from a Debug build and are only indicative; memory figures are not affected.

## Summary

Nothing in scope leaks the document or the scan off the machine, the licence gate in the font installer holds, the ZIP/XML writer escapes markup, Word is launched without a shell, and the mobile app asks for no storage permission and has backup off. The existing tests (`UnitTestsMobile`, and `officeExport textRedline fontNames fontInstaller scanFont scanTextEdit` of `UnitTestsSignatureBuilder`) pass here.

Three things need fixing before the features are trusted:

1. **The mobile app aborts on any password-protected PDF** (null password callback, `std::bad_function_call` is not caught). Proven.
2. **The TrueType writer's OS/2 table is 4 bytes too long** (two extra fields), so every field after `sFamilyClass` is misread: `usWinAscent` becomes 65286/1000 em, Qt reports an ascent of 2611 px for a 40 px font, and the "Build Font from This Scan" preview is empty. Proven.
3. **Excel export turns things that are not quantities into numbers**: `0.125` -> 125, `3.142` -> 3142, `18.10.2026` -> 18102026, phone/ID/account numbers lose their leading zero. Proven.

Plus a path-traversal/registry-injection gap in the font installer that only a compromised catalogue can exploit (proven with a hostile loopback catalogue), stale invisible text after a scan edit, a plugin migration that re-enables plugins the user disabled, Share sending the stale on-disk file, and smaller items below.

## Findings

### High

#### H1. Mobile app crashes (std::terminate) on an encrypted PDF
- `Pdf4QtMobile/pdfmobilecontroller.cpp:91` - `pdf::PDFDocumentReader reader(nullptr, nullptr, true, false);` passes an empty `std::function` as the password callback. `PDFStandardSecurityHandler::authenticate` (`pdfsecurityhandler.cpp:1447`) calls it as soon as the empty user password fails; `PDFDocumentReader` only catches `PDFException` (`pdfdocumentreader.cpp:281,309,559,625,757,815`).
- Scenario: a user opens a bank statement or a contract that has an open password. The app dies without a message.
- Evidence (proven): a probe built a 1-page AES-256 document with user password "pw", wrote it, and read it with the same constructor arguments as the controller: `terminate called after throwing an instance of 'std::bad_function_call'`.
- Fix: pass a callback that returns an empty string and sets `*passwordObtained = false` (then the reader reports "cancelled" and `setStatus` tells the user), or add a password field to the open flow. Also consider catching `std::exception` in `PDFDocumentReader::readFromBuffer` so a library exception never kills the process.

#### H2. Scan font: OS/2 table written with two extra fields, metrics garbage, preview invisible
- `Pdf4QtLibCore/sources/pdfscanfontbuilder.cpp:733-736`. The loop at 733 already writes the 10 sub/superscript and strikeout fields; lines 734 ("strikeout size") and 735 (`xHeightUnits / 2`) are then written again before `sFamilyClass`. The table is 100 bytes instead of 96 for version 4 and every later field is shifted by 4 bytes.
- Effect as parsed by a reader (proven by parsing the written font): `achVendID` = NULs, `fsSelection` = 0x4150 (STRIKEOUT + reserved bits), `usFirstCharIndex` = 17478 > `usLastCharIndex` = 64, `sTypoDescender` positive, `usWinAscent` = 65286, `sxHeight` = 0. Qt (FreeType backend uses the Win metrics) then reports for the font at 40 px: ascent 2611, descent 8, height 2619.
- User-visible: `ScanTextPlugin/scantextdialog.cpp:384` draws the preview with `AlignVCenter` in a 60 px strip; with that ascent the baseline lands thousands of pixels below the strip. Probe: 0 dark pixels in the preview. The edit itself still lands correctly because `apply()` uses `tightBoundingRect` (ink based), so the test `scanFont` does not catch it. The embedded font also carries these metrics into the PDF's FontDescriptor. Whether Windows' font validation (GDI/DirectWrite via `addApplicationFontFromData`) accepts a 100-byte v4 OS/2 with `usLastCharIndex < usFirstCharIndex` is unverified here - plausible rejection.
- Fix: delete lines 734-735 (keep 736 as `sFamilyClass`), then the table is 96 bytes and the fields align. Add a test that parses the OS/2 table (length 96, `usWinAscent` = ascender, `usFirstCharIndex` <= `usLastCharIndex`) and a test that `QFontMetricsF(font).height()` is within 2x the em.

#### H3. Excel export: decimal/thousand heuristic produces wrong numbers
- `Pdf4QtLibCore/sources/pdfofficeexporter.cpp:224-239` (`parseNumber`): a separator followed by 3+ digits is taken as a thousands separator, and anything matching `^-?[0-9][0-9.,]*$` becomes a number after spaces are removed.
- Proven outputs: `0.125` -> 125; `3.142` -> 3142; `12.500` -> 12500; `18.10.2026` -> 18102026; `2026.10.18` -> 202610.18; `1.2.3` -> 12.3; `011 234 5678` -> 112345678; `0821234567` -> 821234567; SA ID `8601015009087` -> 8.601015009e+12 (written as `<v>8.601015009087e+12</v>`).
- Scenario: a quotation with "Rate 0.125", a date column in dd.mm.yyyy, a phone or ID number column: the workbook silently holds different values than the PDF. For a tool meant for contracts and quotes this is a correctness problem, not cosmetics.
- Fix: be conservative. Only treat a separator as thousands when the groups are exactly 3 digits and consistent (`1 234 567,89`, `1,234,567.89`); otherwise a single separator is the decimal point whatever the digit count; more than one kind of separator in a non-grouped pattern (`18.10.2026`, `1.2.3`) is text; keep leading-zero strings (`0821...`, `011 ...`) and strings of 11+ digits as text. Extend `officeExport` with these cases.

### Medium

#### M1. Font installer: catalogue data goes unsanitised into file names and the registry; any URL scheme is fetched
- `pdffontinstaller.cpp:270` builds `<installDir>/<family>-<style>.ttf` from `font.family` as returned by the catalogue JSON; `:287` writes `"<family> <style> (TrueType)"` as a registry value name under `HKCU\...\Fonts` (QSettings turns `/` and `\` in the name into subkeys); `:189` builds the trial cache file name from the *requested* family (which comes from the PDF's `/BaseFont` or the dialog) before any check; `:169` fetches whatever URL the JSON gives (`file://`, `http://`, any host).
- Mitigation that exists: `find()` only returns an entry whose family equals the requested one, the API is HTTPS with certificate validation, and Qt's default redirect policy refuses HTTPS->HTTP. So exploitation needs a compromised or spoofed catalogue (or a user on a network with an injected CA). Defence in depth is missing, not the first line.
- Evidence (proven, hostile loopback catalogue via `setTestConfiguration`): family `../../evil` with ttf URL `file:///etc/hostname`: `download()` returned the 12 bytes of `/etc/hostname`, `install()` returned success and wrote `/tmp/evil-Regular.ttf`, outside the install directory.
- Fix: whitelist the family (`[A-Za-z0-9 ]`, max ~64 chars) before using it as a file or registry name, or derive the file name from `font.id` (the catalogue slug) instead; require `QUrl::scheme() == "https"` and a known host (`api.fontsource.org`, `cdn.jsdelivr.net`) for every fetch; cap the body (`setMaximumRedirectsAllowed`, and reject replies over e.g. 10 MB, since `get()` at `:80` reads everything into memory).

#### M2. Scan text edit leaves old words in a foreign OCR layer
- `pdfscantexteditor.cpp:158` only removes the program's own OCR layer (`PDFOCRTextLayerWriter::getOwnLayerContentReference`, detected through PieceInfo). A scan OCR'd by the scanner software, Acrobat or ABBYY keeps its invisible text: the replaced name/amount is still found by search, copy and the Word export, while the picture shows the new text. The plugin's message (`scantextplugin.cpp:131-136`) says the text layer "still holds them" and points to Edit Page Content, which does not apply to a scan.
- Scenario: Andre fixes a wrong amount on a received scan; the recipient's viewer search still finds the old amount, and the Word export shows it.
- Fix: in `apply()`, also strip the text-rendering-mode-3 (invisible) text objects whose glyph boxes intersect `edit.area` from the page content (the text layout generator already gives the boxes), or, simpler, remove all invisible text on the page and tell the user to run OCR again (as is done for the own layer).

#### M3. Plugin migration re-enables plugins the user disabled
- `pdfprogramcontroller.cpp:2226` `m_knownPlugins = settings.value("KnownPlugins", m_enabledPlugins)`; `:2923-2931` any plugin not in `KnownPlugins` is treated as new and enabled.
- Scenario: an existing user had disabled e.g. Dimensions (so `EnabledPlugins` lacks it) and updates to this build: there is no `KnownPlugins` key, known defaults to the enabled list, Dimensions is "new" and is switched back on, and the result is written back as the new truth. One-time, but it is exactly the case the setting was meant to avoid.
- Fix: when `KnownPlugins` is absent, seed it with the plugins that existed before the fork (every plugin on disk except the ones this fork added), e.g. a constant list `{ "ScanTextPlugin", "SignaturePlugin"... }` of genuinely new plugin names, and only those count as new.

#### M4. Share sends the stale file on disk when there are unsaved changes
- `pdfprogramcontroller.cpp:1621-1640` checks only `QFile::exists(originalFileName)` and shares it. The controller knows about unsaved changes (`m_undoRedoManager->isCurrentSaved()`, used at `:1436, :2360, :2763`) but Share does not ask.
- Scenario: fill in, initial, sign, then Share: the recipient gets the version before the signature. Export to Word at least says so in its dialog; Share says nothing.
- Fix: if `!isCurrentSaved()`, prompt "Save first?" (and save) before sharing. Same for "Open in Word" (`:1665`), which has only a sentence in the dialog.

#### M5. XML export breaks on U+FFFE / U+FFFF in the text
- `pdfofficeexporter.cpp:56` strips control characters below 0x20 but not the non-characters U+FFFE/U+FFFF (illegal in XML 1.0). A PDF whose ToUnicode CMap maps a glyph to one of them (malformed or malicious) yields a .docx/.xlsx that Word/Excel refuse as corrupt.
- Evidence (proven): a page line containing U+FFFE produced `word/document.xml` and `sheet1.xml` that `xml.dom.minidom` rejects (`not well-formed (invalid token)`). Lone surrogates are fine (Qt writes U+FFFD).
- Fix: also drop `0xFFFE`, `0xFFFF` (and `0xD800-0xDFFF` to be safe) in `escape()`.

#### M6. Redline: Myers trace is O(D x (N+M)) memory, and the whole diff blocks the UI
- `pdftextredline.cpp:85` copies the full `v` vector (2 x min(N+M, 6000) + 3 ints) on every round, up to 6000 rounds.
- Evidence (proven): two unrelated 3000-word texts: maxrss grew by 206 MB (peak about 300 MB) and took 12 s in the Debug build; 1500 words: 68 MB. The cap at `MAXIMAL_EDIT_DISTANCE` bounds it at about 290 MB, so this is a resource issue, not a crash; it runs on the GUI thread with a wait cursor and no progress or cancel.
- Fix: store only the `d+1` relevant entries per round (`v[offset-d .. offset+d]`) - that halves the memory and removes most of the copying - or use the linear-space variant; run it in `QtConcurrent` with the progress dialog like other long operations.

#### M7. Font name parsing: `TimesNewRoman,Bold` -> "Times New"
- `pdffontidentifier.cpp:198` strips `Roman` (and `Regular`, `Pro`, ...) from the end of the family before the alias lookup, so names without a `PSMT` suffix lose their last word.
- Proven: `TimesNewRoman` -> `Times New`, `TimesNewRoman,Bold` -> `Times New` (bold), `TimesNewRoman,BoldItalic` -> `Times New`; `Heavyweight` -> `weight` (style words removed inside a word, `:186`). `TimesNewRomanPSMT` and `TimesNewRomanPS-BoldMT` are fine, which is what the test covers. `TimesNewRoman,Bold` is the form older Word and many print drivers write, so the "named by the PDF" row and the installed check are wrong for the most common serif font.
- Fix: run the alias table before stripping the suffixes, and only strip `Roman` when what remains is still in the alias table or installed; match style words at word boundaries (`\b` or CamelCase boundary).

### Low

- **L1. No size limit on downloads** - `pdffontinstaller.cpp:68-88` reads any reply fully into memory (30 s transfer timeout only). A hostile/compromised CDN could feed gigabytes. Cap at a few MB (a Latin TTF is < 1 MB).
- **L2. Fonts come from a third-party CDN without integrity** - the Fontsource API hands out `cdn.jsdelivr.net` URLs (checked live: `.../arimo@latest/latin-400-normal.ttf`) and no hashes. TLS is the only integrity. Acceptable, but document that jsDelivr sees the user's IP, and pin the two hosts (see M1).
- **L3. Trial cache file name from the requested family** - `pdffontinstaller.cpp:189` opens `<cache>/<family with _>-400-normal.ttf` for reading before any validation; a `/BaseFont` of `../../../../etc/hostname` (through "Install Font..." on the PDF-named row) reads that file and tries to load it as a font. Harmless today (it fails to parse and nothing leaves the machine), but the same sanitisation as M1 closes it.
- **L4. ZIP writer has no zip64 and a 16-bit entry count** - `pdfofficeexporter.cpp:271-272,304-305`. A document with more than 65,530 pages gives an xlsx whose central directory count wraps; sizes over 4 GiB truncate silently. Add a guard that fails with a message beyond 60,000 entries / 4 GiB.
- **L5. Export merges columns of multi-column pages** - `extract()` groups all characters of a page by y, so two-column text interleaves line by line in Word, and the redline (which uses the same words) sees a layout change as a text change. Stated as a design limit for Word; for the redline consider the text flows (`SeparateBlocks`) in flow order instead.
- **L6. Mobile: signed file overwritten without asking, viewer not refreshed** - `pdfmobilecontroller.cpp:277` truncates an existing `<name>_SIGNED.pdf`; after signing, the page view still shows the unsigned document and a second Sign re-signs the original, so a countersignature on the phone is not possible without reopening the `_SIGNED` file. For a content URI, `url.fileName()` is the provider's id, so the output is e.g. `1234_SIGNED.pdf`.
- **L7. Mobile: password kept in memory** - the .pfx password is captured by value in the signing lambda and stays in the `TextField` for the session. Fine for a phone app (nothing persisted); clear the field after a successful sign.
- **L8. Android manifest** - `allowBackup="false"`, no storage permission, only the launcher activity exported (required): correct. Missing: a `VIEW`/`application/pdf` intent filter, so "Open with Andre PDF" never appears in other apps; and `android:fullBackupContent`/`dataExtractionRules` are irrelevant with backup off.
- **L9. Scan font harvest allocates a page-sized `visited` bitmap per word** - `pdfscanfontbuilder.cpp:66` (`visited(ink.size())` = 8.4 MB zeroed per word at 300 dpi A4). 500 words -> 4 GB of memset per page: seconds of wait, not a leak. Allocate once per page and clear only the word's rectangle.
- **L10. Font installer partial install on error** - `install()` returns false after a later style fails, leaving Regular registered; acceptable, but say "Regular installed, Bold failed" rather than a bare error.
- **L11. `getExportFileName` for a never-saved document** proposes `/<empty>.docx`; use the Documents folder when `originalFileName` is empty.

## Verified as correct

- **Licence gate**: `download()` (`pdffontinstaller.cpp:96-146`) refuses anything not in the OFL/Apache/UFL/MIT/CC0/GPL-with-font-exception list before any font bytes are fetched; both `loadForTrial` and `install` go through it; the dialog never offers a download for a commercial family (`ensureInstalled`), only the metric-compatible substitutes. Fontsource API shape (`/v1/fonts?family=`, `variants.<weight>.<style>.latin.url.ttf`) matches the live API (checked with one GET).
- **TLS/redirects**: `QNetworkAccessManager` default certificate verification; any TLS error is a reply error and the data is discarded; Qt 6's default `NoLessSafeRedirectPolicy` refuses HTTPS->HTTP. Nothing but the family name is sent out; the scan never leaves the machine (`matchScan` is local).
- **Word launch**: `QProcess::startDetached(path, { file })` with an argument list, no shell; the path comes from HKLM App Paths (admin-writable only) and must exist.
- **XML/ZIP**: `<`, `>`, `&`, `"` escaped; cells are `inlineStr`, never formulas (no `=cmd|...` injection into Excel); CRC-32 and sizes correct (python `zipfile.testzip()` passes in the existing test); `[Content_Types].xml`, `_rels/.rels`, workbook rels are consistent.
- **parseNumber** handles the cases the ledger promised (`1 234,56`, `1,234.56`, `R 250.00`, `-12`, `2026-10-18` as text, `1,5` -> 1.5).
- **Myers diff correctness**: prefix/suffix skip, backtracking from the per-round snapshots, index bounds (`offset +- d` within `2*maximal+3`), the sentinel and the "replaced as a whole" fallback beyond the cap; existing test cases pass.
- **Size reducer**: wraps upstream's `PDFImageOptimizer` (`keepOriginalIfLarger`, bitonal never below 200 dpi) and `PDFOptimizer::All`; the controller compares written sizes and keeps the original when not smaller; `confirmRewriteOfSignedDocument` (default No) now also guards Optimize, Optimize Images and Sanitize; `hasExistingSignatures` sees every signature field with contents, including a DocMDP certification.
- **TrueType writer, apart from H2**: table directory (searchRange/entrySelector/rangeShift), 4-byte padding, table checksums and `checkSumAdjustment`, `head` (54 bytes, long loca), `hhea` (36), `maxp` v1.0 (32), `post` v3 (32), `cmap` format 4 (segments sorted, final 0xFFFF, idDelta mod 2^16), glyph flags (on-curve, 16-bit deltas), contour orientation (outer clockwise after the y flip, test checks areas), `lsb == xMin` with head flag bit 1. The font loads in Qt/FreeType and `QRawFont` reports the family and the character coverage.
- **Scan font tracing**: edge-following on pixel corners always terminates (an edge is erased per step); holes get the opposite orientation; Douglas-Peucker on the closed outline; empty bitmaps yield an empty glyph, not a crash.
- **Scan text edit**: area covered in the median paper colour, new text centred on the ink rectangle and left-aligned with it, font embedded by the content builder (test checks font, size within 2 pt, position within 2 pt); signed documents prompt before the edit.
- **Share sheet (by reading)**: one `DataRequested` handler per window, previous token revoked, deferral completed on both paths, `hresult_error` caught and the fallback (open the folder) used.
- **Execution policy change**: `PDF4QT_EXECUTION_PARALLEL` falls back to `seq` only on libc++; `pdfvisitor.h` is the only user.
- **Third-party notices**: the CMake assembly reads every `3rdparty_licenses/*` and every vcpkg port `copyright` except `vcpkg-*` helpers; the About dialog lists PDF4QT first and links to the notices file.
- **Mobile threading**: the image provider renders on a worker thread under `m_mutex`; `open()` swaps the document under the same mutex and bumps `revision` so QML reloads; no lock is held across a signal that QML answers with a locked call (checked the bindings in `Main.qml`).

## Tests: gaps worth closing

- `scanFont`: parse the written OS/2 table and check `QFontMetricsF::height()` (would have caught H2).
- `officeExport`: the decimal/ID/date cases of H3, and a U+FFFE character (M5).
- `fontNames`: `TimesNewRoman,Bold`, `TimesNewRoman` (M7).
- `fontInstaller`: a family with `../` and a `file://` URL must be refused (M1); a reply over the size cap (L1).
- Mobile: open an encrypted PDF (H1).
- Controller: a settings fixture without `KnownPlugins` and a disabled plugin stays disabled (M3) - needs the GUI build.
