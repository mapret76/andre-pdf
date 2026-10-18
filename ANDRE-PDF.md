# Andre PDF

A free Windows (and later Android) PDF editor that replaces the paid parts of
Adobe Acrobat Pro / PDF-XChange Editor Plus that Andre actually uses. It is a
fork of [PDF4QT](https://github.com/JakubMelka/PDF4QT) (MIT) by Jakub Melka.

This file is the project ledger: decisions, the roadmap, and a dated progress
log. Keep it current in the same commit as the work it describes. It is never
git-ignored.

## AUTOPILOT - RESUME HERE

Andre, 2026-10-18: **"Finish this project unattended. Remember to give credit to
all the GPL and other software that is used."** Sessions run without stopping to
ask. Every decision taken on his behalf goes into the Decisions table, marked
*(autopilot)*, so he can reverse it. Work the queue top to bottom, and for each item:
implement -> build `build-full` (and `build-core` tests) -> commit + push ->
tick it here in the same commit. Self-wakeups continue the work: a Claude Routine
fires into the session (`trig_01CP4nHgdsKiRXKYaTL22aVP`, "Andre PDF autopilot",
every 2 hours at :25 UTC, session `session_01T6oBgM8WAdj7LjGHThvWHP`), and the Routine is
deleted when the queue is empty.

Gates that only Andre can pass (installing on his PC, Acrobat Reader checks,
his own certificate, an Android phone) are listed under "Waiting on Andre". They
do not block the queue.

Credits rule: every third-party component that is **distributed**, or whose
**code or data we adapted**, gets an entry in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)
(name, author, licence, URL, what we use it for) and its licence text in
`3rdparty_licenses/`, and it is shown in Help -> About. Add the entry in the same
commit that brings the component in.

Queue:
- [x] A1 Credits: THIRD-PARTY-NOTICES.md covering everything shipped today, licence texts, About dialog
- [x] A2 Phase 2.4: QR code + SHA-256 fingerprint of the pre-signing document in the visible stamp
- [x] A3 Phase 2.5: one "Sign & Lock" action (optional encrypt -> certify -> re-open -> verify)
- [x] A4 Rebrand to "Andre PDF" (window titles, About, installer product name; internal settings ids kept)
- [x] A5 Phase 3.1: signature library - 4 slots, import from a scanned image (background removed), DPAPI-protected on Windows
- [x] A6 Phase 3.2: initial every page (chosen slot, position, skip pages)
- [x] A7 Phase 3.3: find signature lines (existing fields, "Signature"/"Signed" labels, underscores), sign several, checklist; Lock disabled until all are signed; the last one locks (field /Lock /P)
- [x] A8 Phase 3.4: split a contract pack (by bookmarks / page ranges / detected first pages) + batch initial-sign-lock
- [x] A9 Phase 4.1: size reduction before signing (on PDFOptimizer + image optimizer), blocked after the first signature
- [x] A10 Phase 4.2: export to Word (open in M365 Word) and Excel (tables -> .xlsx)
- [x] A11 Phase 4.3: Share: Windows Share sheet (email/WhatsApp), mail with attachment
- [x] A12 Phase 4.4: highlight/comment polish; contract redline report in DocDiff
- [x] A13 Phase 6: font identification, installed check, find/download/install, embed
- [ ] A14 Phase 5: Android - Pdf4QtLibCore on arm64 in CI, then Qt Quick viewer + sign with .pfx, APK from CI
- [ ] A15 Phase 7: scan-font engine
- [ ] A16 Release: MSI + APK from CI, ledger and skill final, user guide in docs/andre/

Waiting on Andre:
- **GitHub `workflow` scope for the vault (blocks A14 Android CI and any change under
  `.github/workflows/`).** The vault's `gh` token has only `repo`, `gist` and `read:org`, so
  GitHub refuses pushes that touch workflow files. Adding a deploy key was refused by
  Claude's permission classifier, so Andre does it once. **On andre-vault:**
  `gh auth refresh -h github.com -s workflow`. It shows a code to enter at
  github.com/login/device. Until then the MSI keeps the file name `JakubMelka.PDF4QT_<version>.msi`.
- Phase 1 gate: install the MSI and open his PDFs.
- Phase 2 gate: certify a PDF with his own certificate, check it in Acrobat Reader.
- Phase 3 gate: a real contract pack: Signature Library (import his four signatures),
  Split && Sign Contract Pack, open each file in Acrobat Reader.
- Phase 6 gate: 20 of his scans: OCR, then Scan Text -> Edit Scanned Text on a word of
  each; the right family named for at least 16, edits unnoticed at 100% zoom.

## Requirements (as Andre stated them, 2026-10-17/18)

1. Installs on the Windows desktop.
2. OCR, then edit text in the original font.
   *Added 2026-10-18:* when the user picks OCR'd text to edit, the app identifies
   the font in depth, checks whether it is installed, and if not, tells the user and
   searches for the font to download and install, so the edit uses the same font.
3. Add images, text boxes and signatures, saved into the PDF.
4. Digitally sign with his personal certificate (selectable), with a visible box
   that lets a reader verify authenticity.
5. Lock the PDF afterwards.
6. Four signatures scanned from an image, kept for reuse.
7. Initial every page of a contract.
8. Sign several signature lines in one PDF (contract packs arrive as one file);
   the document cannot be closed until every line is signed.
9. Split a PDF into separate files that are each signed on their own.
10. Highlight and remove highlights; add comments; save with both kept on top.
11. Reduce the size of a PDF.
12. Export to Microsoft Word and Microsoft Excel.
13. Share the document by email or WhatsApp.
14. Compare files.
15. An Android version of the same app.

Priority: contract signing first. The mapping of each requirement to a phase is
in the Claude skill (`docs/andre/claude-skill/SKILL.md`, §1) and the full research
behind it in [docs/andre/options-report.md](docs/andre/options-report.md).

## Decisions (and why)

| Date | Decision | Why |
| --- | --- | --- |
| 2026-10-18 | Fork PDF4QT, do not write an editor from scratch | MIT licence (relicensed from LGPL in Apr 2025), mature C++20/Qt 6 core, already signs from the Windows `MY` certificate store, has RFC 3161 timestamps, encryption, annotations, OCR (Tesseract) and a document compare tool (DocDiff). The gaps for our needs are bounded. |
| 2026-10-18 | Public GitHub fork `mapret76/andre-pdf` | GitHub Actions minutes are unlimited for public repos, and the Windows build needs a Windows runner (we develop in the Linux vault). MIT code is public anyway. **Never commit secrets, certificates or real contracts.** Test PDFs must be synthetic. |
| 2026-10-18 | `master` is our branch; upstream is merged in, never rebased onto | Keeps history honest and lets us pull upstream fixes with `git fetch upstream && git merge upstream/master`. |
| 2026-10-18 | No reverse engineering of PDF-XChange or Acrobat binaries | Their EULAs forbid it (PDF-XChange EULA 2026 s3(V)). Every feature is specified in ISO 32000-2 (free since 2023) and ETSI PAdES; we study output files only, never their code. |
| 2026-10-18 | Contract signing is built first | It is the feature Andre needs most and the one no free tool does end to end. Font-faithful scan editing is last (hardest, least urgent). |
| 2026-10-18 | *(autopilot)* Credits: `THIRD-PARTY-NOTICES.md` in the repo, `THIRD-PARTY-NOTICES.txt` assembled at install time (our list + `LICENSE` + `3rdparty_licenses/` + every vcpkg `copyright` file), one row per component in Help -> About | Andre asked for credit to all GPL and other software. Taking the texts from vcpkg at build time keeps them exact for whatever versions are shipped. Qt is LGPL-3.0 and stays dynamically linked and replaceable. |
| 2026-10-18 | *(autopilot)* Verification stamp = QR code + text; the QR holds plain text (signer, UTC time, lock level, SHA-256 of the certificate, SHA-256 of the file before signing) | Any phone QR scanner shows it, no server needed. The file fingerprint is checkable because the signature is appended to the original bytes. An online "verify" page would need a hosted service, so there is none. |
| 2026-10-18 | *(autopilot)* "Sign & Lock" is the Sign dialog itself, not a second button: it defaults to certify "no changes", can encrypt first (AES-256, password to open, random owner password that is discarded, permissions follow the lock level), and re-reads the saved file and reports what a reader will find | One dialog, one click, and no duplicate flows to maintain. Encrypting after signing would break the signature, so the order is fixed in code. |
| 2026-10-18 | *(autopilot)* Signature library = 4 named slots of transparent PNGs, any slot can hold initials; import crops to the ink, removes the paper by an Otsu threshold with a manual slider, keeps or replaces the ink colour; stored in the user profile, DPAPI-encrypted on Windows (plain file with owner-only permissions elsewhere, `ponytail:` noted) | Matches "four signatures scanned from an image". The slider is the calibration knob for real scans and photos. DPAPI ties the file to Andre's Windows account at no cost. |
| 2026-10-18 | *(autopilot)* Contract signing = the default contract mode of the rules: the library image on every line Andre signs, and **one** certification signature (one PIN) whose visible appearance is the last line, with the stamp. Every found line must be decided ("I sign" / "Other party") before Sign & Lock is enabled; "Other party" lines make the lock P=2 (they can still sign), otherwise P=1 | Requirement 8 ("cannot close until all are signed") becomes "cannot lock until every line is decided", which is enforceable in the workflow. The program window is not stopped from closing. Separate digital signatures per line (field `/Lock`) are deferred until a contract needs them. An existing unsigned signature field gets the image, but the digital signature goes into a new field. |
| 2026-10-18 | *(autopilot)* Word/Excel export written by the program itself (Office Open XML in a stored ZIP, no new library): Word gets the paragraphs and pages, Excel one sheet per page with rows split into cells at gaps of 1.5 em and numbers as numbers (South African "1 234,56" understood). On Windows with Word installed, "Open in Word" (Word's own PDF conversion) is offered first | Andre has Microsoft 365, and Word's conversion keeps the layout better than anything we could ship free. The AGPL tools (pdf2docx on PyMuPDF) stay out. For one table, the existing Select Table tool copies it as CSV for Excel. |
| 2026-10-18 | *(autopilot)* Rebrand only the names users see: Andre PDF Editor / Viewer / Pages / Compare, launcher "Andre PDF", installer "Andre PDF" by Andre Pretorius with a new upgrade code; internal application and organisation names stay `PDF4QT ...` / `MelkaJ` | Settings and the OCR model folders are keyed on the internal names. The new upgrade code installs alongside an upstream PDF4QT install instead of replacing it. |

## Roadmap

Effort is for one developer working with Claude. Each phase ends at a gate that
Andre can test himself.

| Phase | Scope | Effort | Gate |
| --- | --- | --- | --- |
| 1 | Fork, ledger, Windows installer from CI, OCR included | 2 weeks | The MSI installs on Andre's PC and opens his PDFs |
| 2 | **Sign & Lock**: create DocMDP certification signatures (PDF4QT only *reads* them today), verification stamp (signer, time, document hash, QR), encrypt-then-certify from one button | 4-6 weeks | Acrobat Reader shows "Certified, no changes allowed" and flags later edits |
| 3 | **Contract packs**: four-slot signature library from scanned images (DPAPI-protected), initial all pages, find signature lines, sign several lines, Lock disabled until every line is signed, split a pack, batch initial-sign-lock | 5-6 weeks | A 3-contract pack splits, signs and locks in one go |
| 4 | Review tools: highlight/comment polish, size reduction before signing, Word/Excel export, Windows Share (email/WhatsApp), contract redline on DocDiff | 6-8 weeks | Comment, shrink, export, share and compare a pack |
| 5 | Android: 2-week proof of concept of Pdf4QtLibCore on arm64, then a Qt Quick touch UI on the same core; .pfx signing first, Android KeyChain later | 4-6 months | A file signed on Android shows as certified in Reader on a PC |
| 6 | Font-matched editing of OCR'd scans: identify the font of the selected text, check it is installed, offer to find, download and install it (see "Phase 6 design notes") | 4-5 weeks | 20 of Andre's scans: the right family named for at least 16, and edits unnoticed at 100% zoom |
| 7 | Scan-font engine (open ClearScan-style synthesised font) | 6-10 weeks | Zero 6/8 or l/1 glyph swaps on the 20-scan set |

### Rules the app must enforce (from the design work)

- Order is fixed: **split -> reduce size -> initial -> sign each line -> lock last.**
  Anything that rewrites the file after the first signature invalidates it.
- Encrypt **before** certifying, never after.
- In one-signature-per-line mode, "no changes allowed" (DocMDP P=1) goes on the
  **last** signature (PDF 2.0 signature-field `/Lock` with `/P`), otherwise every
  later signature breaks it. If someone countersigns after Andre, lock with P=2.
- If reviewers must comment on a signed contract, lock with P=3 (comments allowed).
- The visible stamp proves nothing by itself; the label says "electronic
  signature" unless the certificate is from an SAAA-accredited provider (ECT Act).

## Phase 2 design notes (Sign & Lock)

Starting point, read from the code on 2026-10-18:

- `PDFDocumentSigner` (`Pdf4QtLibCore/sources/pdfdocumentsigner.h`) already
  writes every new signature as an incremental update, verifies it before
  returning, and grows the reserved space when needed. Multiple signatures are
  covered by `UnitTestsSignatureBuilder`.
- The Signature plugin's `SignDialog` offers visible/invisible signatures,
  signature + RFC 3161 timestamp, and document timestamps; certificates come
  from `PDFCertificateManager` (Windows `MY` store via `signImpl_Win`, or .pfx).
- DocMDP is only *read* (`pdfsignaturehandler.cpp`, and honoured by
  `PDFOCRApplyProcessor`). Nothing writes `/Reference` or the catalog `/Perms`.

Planned changes, in order:

1. `PDFDocumentSigner::Parameters` gains a certification level (none = approval
   signature, or DocMDP P = 1 / 2 / 3). When set, the signature dictionary gets
   `/Reference [<< /Type /SigRef /TransformMethod /DocMDP /TransformParams
   << /Type /TransformParams /P n /V /1.2 >> >>]` and the catalog gets
   `/Perms << /DocMDP <sig dict> >>`. Refuse certification when the document
   already has a signature (ISO 32000-2 12.8.2.2: certification must be first).
   Unit tests in `UnitTestsSignatureBuilder`; manual check in Acrobat Reader.
2. "Lock on signing" for the last of several signature lines: the signature
   field's `/Lock` dictionary with `/P` (PDF 2.0 FieldMDP + DocMDP), so the
   final signature closes the document.
3. `SignDialog`: a "Certify: no changes / form filling / comments" choice,
   defaulting to "no changes" for a single signature.
4. Verification stamp: signer, date/time, SHA-256 prefix of the document and a
   QR code, drawn into the visible appearance.
5. One "Sign & Lock" action: optional size reduction and encryption first, then
   certify, then re-open and verify.

Acceptance: Acrobat Reader (free) shows the document as certified with the
chosen permission, and an edit made afterwards in another tool is reported as
a violation.

## Phase 6 design notes (font identification and install)

Requirement 2 as refined on 2026-10-18. When the user selects OCR'd text to edit:

1. **Identify the font.** Do this in two tiers:
   - *The text already has a font* (a born-digital PDF, or a page with a text
     layer): read `/FontDescriptor /FontName` (strip the `ABCDEF+` subset prefix),
     `/Flags`, `/Panose` and the widths. That gives the exact name.
   - *A pure scan* (the OCR result only has word boxes): crop the glyphs of the
     selected words and of the rest of the paragraph, then match them against
     rendered candidates on x-height/cap-height ratio, stroke contrast, serif
     presence, slant, advance widths and per-glyph shape distance on the common
     letters (a, e, g, R, 1, 6). Candidates are the installed fonts plus the
     Google Fonts catalogue. Show the top 3 with a preview, best first, and let the
     user override. Do not rely on Tesseract's font attributes, because the LSTM
     engine in 5.x no longer reports them reliably.
   - Stays **local by default**. Online identifiers (WhatTheFont and similar)
     would send contract images off the machine (confidentiality, POPIA s72), so at
     most they are an opt-in per use.
2. **Is it installed?** Look it up in `QFontDatabase` by family and style (on Windows
   that covers the system fonts and the per-user fonts).
3. **If not, prompt and search.** "This text is set in *X*, which is not installed."
   - *Free licence* (OFL/Apache, e.g. the Google Fonts catalogue): offer Download
     & install. Install per user, with no admin rights: copy to
     `%LOCALAPPDATA%\Microsoft\Windows\Fonts` and add the `HKCU\Software\Microsoft\Windows NT\CurrentVersion\Fonts`
     value, then `QFontDatabase::addApplicationFont` for this session.
   - *Commercial font* (Helvetica, Frutiger, ...): never download it from an
     unlicensed mirror. Say where it can be licensed, and offer the closest free
     metric-compatible substitute (Arial -> Liberation Sans, Calibri -> Carlito,
     Cambria -> Caladea, Helvetica -> TeX Gyre Heros, Times -> Liberation Serif),
     marked as a substitute.
4. **Edit and embed.** Write the edit with the chosen font embedded as a subset in the
   PDF, so the file reads the same on a machine that does not have the font.

Licences: the Google Fonts metadata and files are free to use; the matcher is ours.
Phase 7 (a font synthesised from the scan's own glyphs) is the fallback when no
candidate matches well.

## Building

**Windows installer (the deliverable):** GitHub Actions workflow
`Windows_MSI` ([.github/workflows/WindowsInstall.yml](.github/workflows/WindowsInstall.yml)), started by hand:

```bash
gh workflow run WindowsInstall.yml -R mapret76/andre-pdf --ref master
gh run watch -R mapret76/andre-pdf
gh run download <run-id> -R mapret76/andre-pdf
```

It produces an MSI, an MSIX and a zip of the binaries. The MSI is **not code-signed**
(the `SIGN_MSI` repository variable is unset; upstream signs with a DigiCert
KeyLocker certificate we do not have), so Windows SmartScreen warns on first run.

**Linux (development in the vault):** `ci.yml` builds and tests on every push to
`master`. A local build in the vault needs Qt 6.9+, CMake, Ninja and vcpkg; see
the upstream notes in [CLAUDE.md](CLAUDE.md) (its Windows paths describe the
upstream author's machine, not ours).

## Where things live

- Working copy: vault `~/projects/andre-pdf`
- Remotes: `origin` = github.com/mapret76/andre-pdf (public fork, second copy off the vault), `upstream` = github.com/JakubMelka/PDF4QT
- Options report: [docs/andre/options-report.md](docs/andre/options-report.md) (markdown export of Andre's private Claude Doc "Free Acrobat-Pro Alternative — Options Report", rev 27; the two diagrams are not in the export)
- Claude skill: `~/.claude/skills/andre-pdf/SKILL.md` on the vault and the PC (kept in step with `~/.claude/tools/sync-claude-skills.sh`, run from the PC). A copy lives in [docs/andre/claude-skill/SKILL.md](docs/andre/claude-skill/SKILL.md) for history: when you edit the skill, copy it here in the same commit.

## Progress log

### 2026-10-18
- Forked PDF4QT at upstream `905cefa` as `mapret76/andre-pdf`, cloned to the vault, `upstream` remote added, Actions enabled on the fork.
- Found that upstream already ships: a Windows MSI/MSIX workflow, Tesseract OCR with built-in `fast` models in the installer, `PDFDocumentSigner` with multiple incremental signatures (tested by `UnitTestsSignatureBuilder`), RFC 3161 timestamps (`UnitTestsTimestamp`), and DocMDP *parsing* in `pdfsignaturehandler.cpp`. Creating DocMDP certification is the first real gap for Phase 2.
- Started the first Windows_MSI run on the unmodified fork to prove the pipeline.
- First Windows_MSI run **succeeded** (run 39118485205, 71 min): MSI 54.1 MB
  (SHA-256 `62c9f12d...929482`), MSIX and a zip. Contents checked: Editor, Viewer,
  PageMaster, Diff, LaunchPad, PdfTool, SignaturePlugin, Tesseract 5.5 +
  Leptonica, built-in `fast` OCR models (eng, deu, spa, rus, ces, slk, chi_*,
  osd; **no Afrikaans** built in - downloadable in the OCR language dialog).
  The Linux CI run on the ledger commit also passed (53 min, includes tests).
- Gate for Phase 1 is now with Andre: install the MSI on his PC (unsigned, so
  SmartScreen warns: More info -> Run anyway) and open his own PDFs.
- Wrote the `andre-pdf` Claude skill (stable knowledge: requirements map, build,
  signing rules, roadmap, hard lines, access traps) and synced it to the vault.
  Exported the options report into `docs/andre/`. Next session resumes in the
  vault: start with the skill, then this ledger. **Open:** Andre's install test
  of the Phase 1 MSI; then Phase 2 step 1 (certification level in
  `PDFDocumentSigner`), which wants a local vault build set up first.

### 2026-10-18 (later) - Phase 2 started
- Local vault build set up for the core library (no vcpkg): Ubuntu 26.04 packages
  (Qt 6.10.2, OpenSSL, freetype, openjpeg, lcms2, TBB, ...) plus blend2d (no JIT)
  and Clipper2 2.0.1 built from source into `~/.local/andre-pdf-deps` (sources in
  `~/projects/andre-pdf-deps-src`). Configure:
  `cmake -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/.local/andre-pdf-deps -DPDF4QT_BUILD_ONLY_CORE_LIBRARY=ON -DPDF4QT_ENABLE_OCR=OFF`.
  `build-*/` is git-ignored. The GUI and plugins are compile-checked by Linux CI.
- **Step 1 done:** `PDFDocumentSigner::Parameters::certification` (None / NoChanges /
  FormFilling / FormFillingAndAnnotations = DocMDP P 1/2/3). Writes the SigRef DocMDP
  reference (`/V /1.2`) into the signature dictionary and catalog `/Perms /DocMDP`.
  Refused with `Result::CertificationNotFirst` when the document is already signed
  or for a document timestamp. Tests `certification` and `certificationMustBeFirst`
  in `UnitTestsSignatureBuilder` (an approval signature after a certification keeps it).
- **Step 3 done:** SignDialog has "Lock (certify)": No changes (default) / form filling
  / form filling + comments / do not lock. Disabled when the document is already signed.
  The choice is remembered.
- **Step 4 partly:** a visible signature now carries a text stamp under the graphics:
  "Electronically signed by <certificate CN>", the signing time, and the lock level.
  QR code and document hash are still to do.
- Step 2 (signature-field `/Lock` with `/P` for the last of several lines) moves to
  Phase 3, where the multi-line flow that needs it is built. Step 5 (one-button
  reduce/encrypt -> certify -> verify) is open.

### 2026-10-18 (evening) - Phase 2 work committed; font requirement added
- The core build had stopped on a missing `fontconfig/fontconfig.h`; installed
  `libfontconfig-dev` on andre-vault. `build-core` now builds. Tests:
  `UnitTestsSignatureBuilder` 47 passed (includes the new `certification` and
  `certificationMustBeFirst`), `UnitTestsTimestamp` 29 passed, 1 skipped (public TSA).
- Committed steps 1, 3 and the text stamp of step 4. The GUI and plugin compile is
  left to Linux CI.
- **Andre refined requirement 2:** before editing OCR'd text, identify the font in
  depth, check it is installed, and if not, prompt the user and search, download
  and install it. Design in "Phase 6 design notes": a local matcher, per-user
  install of free fonts, and a licensed-source pointer or metric-compatible
  substitute for commercial fonts. Phase 6 grows to 4-5 weeks. It stays after
  contract signing (his priority).
- **Open:** Andre's MSI install test (Phase 1 gate); step 4 QR code and document
  hash; step 5 one-button Sign & Lock; acceptance in Acrobat Reader needs a new
  MSI build that includes this work.

### 2026-10-18 (night) - autopilot: A1-A4
- **A1 credits:** `THIRD-PARTY-NOTICES.md` (every distributed library, the code included
  in the tree, the build tools, the standards), `cmake/AssembleThirdPartyNotices.cmake`
  installs `THIRD-PARTY-NOTICES.txt` (WiX component added), LGPL-3.0/GPL-3.0 texts in
  `3rdparty_licenses/`, About lists PDF4QT first plus oneTBB, libpng, Clipper2 and the
  Windows-only vcpkg libraries; About's copyright line credits Jakub Melka.
- **A2 stamp:** `PDFSignatureStamp` (core) draws QR + text; Nayuki's QR Code generator
  (MIT, commit 3c6d0b3) vendored in `Pdf4QtLibCore/sources/qrcodegen/`. Test
  `verificationStamp` decodes the drawn QR with `zbarimg` (installed on the vault).
  If there is no room under the signature graphics, the stamp goes above them.
- **A3 sign & lock:** `Parameters::encryptionUserPassword`, `getEncryptionPermissions`,
  `checkSignedFile` (core). Sign dialog: encrypt option + confirm, disabled when already
  signed. After saving, the file is re-read from disk and the result shown. Tests
  `encryptedCertification` (wrong password, permissions, tampering, refusal on a signed
  file) and `dialogLockAndEncryption`.
- **A4 rebrand:** display names, MSI product/manufacturer/folders/shortcuts/upgrade code,
  MSIX identity. The MSI file name (set in the workflow) waits for the `workflow` scope.
- Full local build set up (`build-full`, Release, GUI + OCR with the Ubuntu Tesseract via
  a CMake shim in `~/.local/andre-pdf-deps/lib/cmake/Tesseract`; packages
  `qt6-speech-dev libtesseract-dev libleptonica-dev libfontconfig-dev zbar-tools`).
  Fixed: `UnitTestsSignatureAppearance` used the two-argument `SignDialog`
  constructor; `canCertify` now defaults to true.
- Full suite on the vault (`ctest` in `build-full`): 33 of 34 pass. `UnitTestsFontEncoding::test_standard_font_zapf_dingbats`
  fails on the vault only (the Ubuntu URW clone `D050000L.otf` is found but yields no glyphs); the code is
  upstream's and untouched, Linux CI is the reference. Investigate in Phase 6 (fonts).

### 2026-10-18 (night) - autopilot: A5 signature library
- `PDFSignatureLibrary` (core): slots, serialization, DPAPI protect/unprotect, load/save
  (`QSaveFile`, owner-only), `extractSignature` (Otsu with the middle of tied levels, so a
  clean two-tone scan works; soft edge band; crop + margin; size limit), `toPng`.
- Plugin: "Signature Library..." dialog (preview on a checkerboard, name, import, clear),
  import dialog (live preview, threshold slider with Automatic, ink colour), four
  "Place <name>" tools in the Signature Toolbox (disabled while the slot is empty).
- Tests: `signatureLibrary`, `signatureExtraction` (noisy and clean scans),
  `signatureLibraryDialogs`, `signatureImageKeepsTransparency` (the PNG reaches the PDF
  with its mask, so it does not cover the signature line).

### 2026-10-18 (night) - autopilot: A6 initial every page
- `PDFPageInitials` (core): position on the page as displayed (bottom right/centre/left,
  top right/left), honouring `/Rotate`; width and margin in points; skipped pages; draws
  into the page content. Test `initialsOnEveryPage` renders an upright and a rotated
  page and checks the pixels.
- **Upstream bug fixed** in `PDFPageContentStreamBuilder::end` (place before/after): a
  page without its own `/Resources` lost the resources of the new content (a null object
  was merged over them), and a page inheriting `/Resources` from the page tree lost them
  for its old content. Now the inherited resources are looked up and only a dictionary
  is merged. This also fixes "Sign Electronically" on such pages. Worth offering
  upstream as a pull request.
- Plugin: "Initial All Pages..." (choose the slot, position, width/margin in mm, skip the
  last page by default, skip a page list), refused on an already signed document.
  Test `initialsDialog`.

### 2026-10-18 (night) - autopilot: A7 contract signing
- `PDFSignatureLineFinder` (core): unsigned signature fields (page through `/P` or the page
  `/Annots`), underscore/dot runs labelled as a signature on the same line or by the
  nearest line under/over it (English and Afrikaans, name/date/place lines excluded),
  a "Signature:" label ending a line; lines under a field merge with it; ordered top-down.
  Tests `signatureLineLabels`, `signatureLineFinder` (real text written by Qt's PDF writer).
- `PDFPageInitials::drawImages` / `getFittedDisplayRectangle`: an image fitted onto lines.
- Plugin "Sign Contract...": the checklist dialog (`ContractSigningDialog`, double-click
  shows the page, "I sign all"), images on the lines, the last line in the scene, then the
  Sign dialog with the lock level preset. Test `contractSigningChecklist`.

### 2026-10-18 (night) - autopilot: A8 split and sign a contract pack
- `PDFContractPack` (core): parts from the top level bookmarks, from the page numbering
  restarting at "Page 1 of N" / "1 of N" / "1/N" / "Bladsy 1 van N", or from page ranges
  (checked for the document and for overlaps); extraction of a part as its own document;
  safe file names.
- `PDFSignatureStamp::createVisibleSignatureField` (core): a visible signature field from an
  image and the stamp, with no interactive scene (for batches, and later Android).
- Plugin "Split && Sign Contract Pack...": parts table (bookmarks by default, page numbers
  or ranges on demand), initials slot (not the last page of each contract by default),
  signature slot, "every line" or "only lines labelled with ...", output folder and name.
  One Sign dialog for the certificate and password, then for each part, in the order of
  the rules: extract -> initial -> images on the lines -> certify with the visible signature
  on the last line (P=2, when lines are left for another party) -> write (never over an
  existing file) -> read back and check; a report lists every contract.
- Tests: `contractPack` (split, extract, sign the part with a visible field, check) and
  `packSigningDialog`. **Phase 3 is complete in code.** Its gate (a real three-contract
  pack in Acrobat Reader) waits for Andre.

### 2026-10-18 (night) - autopilot: A9 reduce file size
- `PDFSizeReducer` (core): presets e-mail (150 dpi, JPEG 75), smallest (110 dpi, JPEG 60),
  printing (300 dpi, JPEG 85); bitonal scans never below 200 dpi; image optimizer, then
  every object optimization. Test `sizeReduction` (a phone-scan page more than halves).
- Editor: Tools -> "Reduce File Size..." picks the preset, shows "before -> after (n%
  smaller)" and applies on Yes. Optimize, Optimize Images, Sanitize and Reduce File Size
  warn on a signed document (the rewrite breaks the signatures; default No).

### 2026-10-18 (night) - autopilot: A10 Word and Excel export
- `PDFOfficeExporter` (core): characters of a page regrouped into lines by position
  (tables are laid out as column blocks), word spaces from the text, cells at gaps over
  1.5 em, `parseNumber` (R, spaces, comma or point decimals), `writeDocx`, `writeXlsx`,
  `createZip` (stored entries, CRC from zlib). Test `officeExport` validates both files
  with Python's zipfile and XML parser.
- Editor: File -> "Export to Word..." (Windows: "Open in Word" when Word is registered
  under App Paths, else the text export) and "Export to Excel...", both open the result.

### 2026-10-18 (night) - autopilot: A11 share
- E-mail with the file attached already existed (File -> Send by E-Mail, Simple MAPI:
  Outlook or the default mail program).
- New File -> "Share..." opens the Windows share sheet (`PDFShareSheet`, C++/WinRT,
  `IDataTransferManagerInterop::ShowShareUIForWindow`, the file as a storage item).
  WhatsApp Desktop, Teams, Outlook and nearby sharing appear there when installed.
  `WindowsApp` is linked on MSVC. Without C++/WinRT (other systems, MinGW) the folder of
  the file opens instead. It compiles only on Windows: checked by the Windows MSI build.

### 2026-10-18 (night) - autopilot: A12 comments and redline
- Highlights (add/remove), comments and their replies are upstream PDF4QT annotation
  tools with appearance streams, so other readers show them, and they are covered by its
  annotation test suites. Nothing needed changing. Deleting a highlight is Delete
  Annotation.
- `PDFTextRedline` (core): word diff (Myers O(ND), common prefix/suffix skipped, limit of
  6000 edits) of the text of two versions; changes are deleted / inserted / replaced, with
  the pages and six words of context; a report PDF (deleted red struck through, inserted
  blue underlined, A4, over as many pages as needed, laid out at 72 dpi). Test
  `textRedline` (changes, empty case, the report's own text read back, pagination).
- Compare app: "Create Redline Report..." (left = old version, right = new version),
  saves and opens the report. **Phase 4 is complete in code.**

### 2026-10-18 (night) - autopilot: A13 font-faithful editing of scans (Phase 6)
- `PDFFontIdentifier` (core): PDF font names to installed families (subset prefix, PostScript
  names, glued style words, squeezed names matched against the installed families);
  installed check (foundry suffixes "[urw]" and generic aliases such as "Serif" left out);
  metric-compatible free substitutes; `matchScan` renders the text in each candidate (4
  styles) and compares ink masks at 40 px height (tolerance of one pixel, ink amount, width
  of the whole text), and gives the pixel size from the ink height; `getScanColors`.
- `PDFFontInstaller` (core): Fontsource API (`api.fontsource.org/v1`, no key), free
  licences only (OFL, Apache, UFL, MIT, CC0, GPL with the font exception), Latin subset
  TTFs; `loadForTrial` (cache, this session only) and `install` per user: Windows
  `%LOCALAPPDATA%\Microsoft\Windows\Fonts` + HKCU Fonts registry + AddFontResource +
  WM_FONTCHANGE, elsewhere ~/.local/share/fonts. A commercial font is never downloaded; its
  free substitutes are named instead.
- `PDFScanTextEditor` (core): render an area, its text (OCR layer), ink rectangle; the edit
  covers the area in the paper colour and writes the new text centred on the old ink,
  in the matched font and size and the ink colour (embedded by the PDF writer). The own
  OCR layer of the page is removed, because it would still hold the old words; the user
  is told to run OCR on the page again. On born-digital text the old text stays under
  the cover and the user is pointed to Edit Page Content.
- New plugin **Scan Text** -> "Edit Scanned Text..." (rectangle tool, dialog: the scan, its
  text, the ranked fonts with their status, fonts named by the PDF first when missing,
  "Search Free Fonts Online..." tries 26 common free document fonts, "Install Font...",
  preview, Replace Text). Installer component added. Plugins added by an update are now
  enabled for existing users ("KnownPlugins" setting).
- Tests: `fontNames`, `fontMatching` (identified among all installed fonts), `fontInstaller`
  (loopback catalogue, licence refusal, install, trial cache), `scanTextEdit` (end to end:
  font, size within 2 pt, position within 2 pt), `scanTextDialog`.
