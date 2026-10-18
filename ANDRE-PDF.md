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
| 6 | Font-matched editing of OCR'd scans | 2-3 weeks | 20 of Andre's scans: edits unnoticed at 100% zoom |
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
