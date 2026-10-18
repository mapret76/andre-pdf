# Andre PDF

A free Windows (and later Android) PDF editor that replaces the paid parts of
Adobe Acrobat Pro / PDF-XChange Editor Plus that Andre actually uses. It is a
fork of [PDF4QT](https://github.com/JakubMelka/PDF4QT) (MIT) by Jakub Melka.

This file is the project ledger: decisions, the roadmap, and a dated progress
log. Keep it current in the same commit as the work it describes. It is never
git-ignored.

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
