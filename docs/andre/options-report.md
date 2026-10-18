# Free Acrobat-Pro Alternative — Options Report

Oct 17, 2026 · @Andre

## Bottom line

You can have four of the five features free this week. The fifth, editing OCR'd scans in the original font, exists in no open-source tool and is roughly 2–3 months of build. Use free tools now, and fork PDF4QT (MIT) to build the rest legally from the open PDF standard.

- **Now, R0:** install [KillerPDF](https://github.com/SteveTheKiller/KillerPDF) to OCR, edit, and add images, text boxes and signatures. Then use [jSignPdf](https://github.com/intoolswetrust/jsignpdf) to sign with your Windows certificate, add a visible box, certify "no changes" and encrypt. Editing a scan covers the old text with a font-matched text box; it is not a true in-font edit.
- **Benchmark to beat:** PDF-XChange Editor Plus is **US$79 once** and does all five, though whether its scanned-text editing matches the original font is unverified. Acrobat Pro is about US$240 a year.
- **Build:** fork [PDF4QT](https://github.com/JakubMelka/PDF4QT), which is MIT-licensed and already signs from the Windows certificate store. Add one "Sign & Lock" step (certification, QR verification box, encryption), then font-matched scanned-text editing, then an open ClearScan-style synthesised font. Then add the contract-pack workflow: a four-signature library, initials on every page, multi-line signing with lock-when-complete, and pack splitting. Then the review tools (highlights, comments, size reduction, Word and Excel export, sharing, compare) and an Android app on the same core. Everything together is about 12–13 months for one developer working with Claude, in seven phases (see Plan). Contract signing is usable on the PC after about 3 months and on Android after about 10.
- **Not doing:** cracking or decompiling PDF-XChange's paid tier. It breaches the EULA, risks criminal liability under the Cybercrimes Act, and is unnecessary: the PDF standard (ISO 32000-2) is free and documents every feature.

## Your requirements, scored

Free software covers the basic needs (1, 3, 4, 5) and parts of the contract-pack workflow (6–9). Editing OCR'd text in the original font (2) exists nowhere outside Acrobat. Free tools cover the contract-pack steps only partly, with no checklist, and making that one smooth flow takes about 5–6 weeks. The review tools (10–14) are mostly free already. No free Android app signs with a certificate (15).

| Requirement | Free today? | Best free source | Difficulty to build |
| --- | --- | --- | --- |
| 1. Installs on the Windows desktop | Yes | All candidates ship MSI or setup installers | None |
| 2. OCR, then edit text in the original font | **No.** OCR yes; editing uses a cover box in a matched system font | KillerPDF (cover-and-replace); Tesseract / PaddleOCR | **High**: font matching 2–3 weeks; synthesised scan font 6–10 weeks |
| 3. Images, text boxes, saved signatures | Yes | KillerPDF, PDF4QT | Low |
| 4. Sign with your own certificate (selectable) + visible verification box | Yes, except a QR verification link | jSignPdf, PDF4QT, KillerPDF (Windows store); pyHanko (QR stamp) | Low: 1–2 weeks to add the QR and verification text |
| 5. Lock afterwards | Yes | jSignPdf: certify "no changes" + encryption in one pass | Low; medium if you are adding certification to PDF4QT |
| 6. Four signatures scanned from an image, kept in a library | Partial: tools import a signature image, but do not clean it up or keep named slots | KillerPDF, PDF-XChange free | Low: 1 week |
| 7. Initial every page of a contract | Partial: image stamp across a page range | Stirling-PDF (unverified on desktop) | Low: 1 week |
| 8. Digitally sign several signature lines, lock only when all are signed | Partial: signatures can be appended one at a time; no line detection or completion check | jSignPdf | Medium: 2–3 weeks |
| 9. Split a pack into separate files, each signed on its own | Yes, as two tools: split, then batch-sign | PDFsam Basic + jSignPdf | Low: 1 week |
| 10. Highlight and remove highlights; comments; save with both kept on top | Yes | PDF4QT, KillerPDF, Acrobat Reader | Low: under 1 week |
| 11. Reduce file size | Partial: command line only | OCRmyPDF, qpdf | Low: 1–2 weeks |
| 12. Export to Word and Excel | Yes, through Microsoft 365 Word and Excel; open-source converters are weaker | Word, Excel; pdf2docx, Docling | Medium: 2–4 weeks |
| 13. Share by email or WhatsApp | Yes, through Windows Share | Windows Explorer | Low: under 1 week |
| 14. Compare two files | Yes, page by page; no contract redline | PDF4QT DocDiff | Medium: 2–3 weeks |
| 15. Android version of the same app | No: free apps sign with images only; Foxit's certificate signing is paid | None | High: 4–6 months, after a 2-week proof of concept |

## What PDF-XChange and Acrobat actually charge for

PDF-XChange's free Editor already does most of what you need without a watermark: OCR, images, signatures, certificate signing, certify and passwords. The paid tiers gate only text editing and editable OCR. Editor Plus costs **US$79 once**, and that is the real price to beat.

In PDF-XChange's [comparison chart](https://www.pdf-xchange.com/pdf-xchange-products-comparison-chart), a licensed feature still works in the free version but stamps a watermark on output ([demo limits](https://www.pdf-xchange.com/product/pdf-xchange-editor/demo_limits)).

| Feature | PDF-XChange free | Editor (US$62) | Editor Plus (US$79) / PRO (US$131) | Acrobat |
| --- | --- | --- | --- | --- |
| OCR scans (Tesseract, searchable text layer) | Yes | Yes | Yes | Standard: searchable only |
| Edit existing text | Watermarked | Yes, born-digital text only | Yes | Standard and Pro |
| Edit **scanned** text after OCR | No | No | Yes: Enhanced OCR (ABBYY engine), "editable text" output; font matching unverified | **Pro** only (US$19.99/month) |
| Images, text boxes, drawn signatures | Yes | Yes | Yes | Standard and Pro |
| Certificate signing (system store or .pfx), custom visible appearance, timestamp | Yes, per the 2026 chart (an older 2023 KB says it needs a licence) | Yes | Yes | Standard and Pro |
| Certify with "permissions after signing" (lock) | Yes, per chart | Yes | Yes | Pro certain; Standard unverified |
| Password and permission security (AES-256) | Yes, per chart | Yes | Yes | Standard and Pro |
| Licence model | Free | Perpetual + 1 yr updates | Perpetual + 1 yr updates | Subscription: Standard US$14.99, Pro US$19.99 a month |

PDF-XChange prices are single-user, from the [official shop](https://www.pdf-xchange.com/shopgt/pdf-xchange-editor.htm), and you keep the version you bought for ever. Acrobat prices are from the [Adobe compare page](https://www.adobe.com/acrobat/pricing/compare-versions.html); South African rand prices did not load. Acrobat Pro costs about US$240 a year, so PDF-XChange Editor Plus pays for itself in four months.

Before relying on the free version for signing, install it and sign a test file. The 2026 chart and the 2023 KB disagree on whether signing is free.

## Reverse engineering: what is off-limits, and the route that gets the same result

Do not crack, patch or bypass PDF-XChange's or Adobe's licensing. You also do not need to: every feature you want is defined in a free, open standard. Studying a product's *output* and building the feature yourself is legal and gets the same result.

| Approach | Status | Why |
| --- | --- | --- |
| Patch the PDF-XChange binary or keygen to unlock paid tools or remove watermarks | **Off-limits** | It breaches the EULA, which is enforceable in South Africa as a contract. It infringes reproduction and adaptation rights under Copyright Act s11B. Software that defeats security measures is a criminal offence under the Cybercrimes Act 19 of 2020, which replaced ECT Act s86. |
| Decompile their code to copy how a feature works | **High risk** | Current Copyright Act s19B allows only backup copies; there is no clear decompilation right. The Copyright Amendment Bill adds a study-and-interoperability exception, and the Constitutional Court upheld it on 26 Jun 2026, but it is **not yet law**. |
| Run a "reverse-engineer anything" AI agent against their installer | **Off-limits for this goal** | The repo is morluto/rea (MIT): an MCP server that drives Ghidra or IDA to take apart binaries. It is legal for software you own or may examine. PDF-XChange's 2026 EULA s3(V) forbids reverse engineering, so using REA on it breaches that clause. Its \~300k stars look inflated, and trojanised copies circulate. |
| Black-box study of **files** Acrobat or PDF-XChange produce (sign, certify, OCR a test PDF, then inspect the objects with `qpdf --qdf` or `pdf-parser`) | **Fine** | These are your own documents. File formats and features are not protected by copyright, only code is. |
| Clean-room build from ISO 32000-2 (PDF 2.0) and ETSI PAdES | **Fine, and recommended** | [ISO 32000-2](https://pdfa.org/sponsored-standards) has been free since Apr 2023, sponsored by Adobe, Apryse and Foxit. It defines signatures, DocMDP locking, appearances, encryption and fonts exactly. |

So the "write the sections from reverse engineering" path becomes: make a reference file with Acrobat Reader or PDF-XChange free, dump its structure, read the matching ISO 32000-2 clause, and implement it on an open library. Use your own feature names: "ClearScan" is an Adobe trade mark. A patent search on font synthesis from scans is still open; see Plan.

## Option A: free tools you can install today

No single open-source app covers all five needs, but two installed together cover four and a half. Use KillerPDF to edit, then jSignPdf to sign and lock. GitHub figures were checked on 17 Oct 2026.

| App | Licence | Stars | Last release | OCR + edit text | Images, text boxes, signatures | Cert signing + visible box | Lock |
| --- | --- | --- | --- | --- | --- | --- | --- |
| [KillerPDF](https://github.com/SteveTheKiller/KillerPDF) | GPL-3.0 | \~4.1k | v1.8.91, 14 Oct 2026 | Partial: Tesseract 5 OCR; edits cover old words with a font-matched text box | Yes, saved into page content | Yes: Windows store, USB tokens, .pfx, editable visible box | Partial: engine supports it, UI only removes passwords |
| [PDF4QT](https://github.com/JakubMelka/PDF4QT) | MIT | \~1.5k | v1.6.0.0, 14 Jun 2026 | Partial: OCR only in unreleased master; editor uses system fonts | Yes | Yes: Windows `MY` store via CryptoAPI, drawn visible box, RFC 3161 timestamps | Partial: encryption yes; cannot create a certification signature |
| [jSignPdf](https://github.com/intoolswetrust/jsignpdf) | MPL-2.0 / LGPL-2.1 | \~480 | 3.2.1, 28 Sep 2026 | No | No | Yes: Windows-MY, PKCS#11, .pfx, drag-placed box, TSA, PAdES-LTA | **Yes**: certify "no changes" and encrypt with permissions in one pass |
| [Stirling-PDF](https://github.com/Stirling-Tools/Stirling-PDF) | MIT core + proprietary desktop shell | \~94k | v3.2.0, 15 Oct 2026 | Partial: OCR not in the free local desktop mode | Yes | Yes: Windows store, visible box | Partial: password only, no certify |
| [Open PDF Studio](https://github.com/OpenAEC-Foundation/open-pdf-studio) | LGPL-3.0 | \~920 | 15 Oct 2026 | Partial: OCR yes, font fidelity unverified | Yes | No: validates only | No |
| [Okular](https://docs.kde.org/stable/en/okular/okular/signatures.html) | GPL-2.0+ | — | 26.08.2 | No | Annotations only | Partial: visible box, but not the Windows cert store | No |
| [LibreOffice Draw](https://help.libreoffice.org/latest/en-US/text/shared/01/signexistingpdf.html) | MPL-2.0 | — | 26.x | No OCR; import breaks layout | Only on re-export | Partial: visible signature is unreliable | Encryption only on export |
| PDFgear (closed freeware) | Proprietary | — | — | Yes | Yes | Partial: own Digital IDs; store unverified | Password only |
| Acrobat Reader / Foxit Reader (free) | Proprietary | — | — | No | Fill & Sign only | Yes, visible | No: certify and encrypt are paid |

**The free workflow, today:**

1. Open the scan in KillerPDF, run OCR, edit the text, and place images, text boxes and your drawn signature. Save.
2. Open the result in jSignPdf. Pick your certificate from the Windows store, drag the visible signature box, and tick certification level "no changes allowed" with encryption and permissions. Sign.
3. Open the output in Acrobat Reader to confirm the signature panel shows it as valid and certified.

**What this does not give you:** Acrobat-quality editing of scanned text. KillerPDF covers the old words and types new ones in a matched system font, and the original text stays underneath. Treat edited files as revised documents, never as redactions.

**Avoid on GitHub:** 0-star repos created on 11 Oct 2026 that promise "Master PDF Editor" or "Soda PDF" downloads look like malware lures.

## Option B: fork an open-source editor and add the missing pieces

Fork PDF4QT. It is MIT-licensed, mature C++/Qt with one long-standing maintainer, and it already signs from the Windows certificate store. The gaps are bounded and well defined.

| Base | What is already there | What you add | Effort | Licence effect |
| --- | --- | --- | --- | --- |
| **PDF4QT** (recommended) | Windows `MY` store signing (`CertOpenStore` + `CryptSignMessage`), PKCS#12, drawn visible signature box, RFC 3161 timestamps, encryption with permissions, content editor, Tesseract OCR in master | 1. Create DocMDP certification signatures (`/Reference` DocMDP transform + catalog `/Perms`); the parser already reads them. 2. Font-matching and synthesised-font editing for OCR text (see the hard-part section). 3. Verification block and QR in the signature appearance. 4. One-click "Sign & Lock" flow | 6–10 weeks for 1, 3 and 4; another 6–10 for 2 | None. You can keep the fork private, or relicense your additions |
| **KillerPDF** | Closest to complete today: Windows store and USB tokens, editable visible box, OCR, font-matched cover-and-replace editing; the engine already has DocMDP and AES-256 | Wire the certify and encrypt options into the UI; make edits use the OCR layer on scans | 2–4 weeks | GPL-3.0 only: your fork must stay GPL if you ever give it to anyone. The repo started in Apr 2026 with one maintainer |
| **jSignPdf** engine as a sidecar | Complete signing and locking: Windows-MY, PKCS#11, TSA, PAdES-LTA, certification, encryption | Call it from whichever editor you choose, as the final "Sign & Lock" step | 1–2 weeks | MPL/LGPL: safe to call or link without opening your own code |

KillerPDF is the faster route to a working tool. PDF4QT is the better long-term base: it carries no copyleft, it has more history, and its architecture can take a true ClearScan-style editor.

Avoid forking Stirling-PDF. Its desktop shell and parts of its engine are under a proprietary licence, OCR needs their cloud or a server, and it has no certify option.

## Option C: build your own app from components

Option C is Option B with the parts drawn out: a PDF4QT-based Windows app where every step except two is an existing open-source library. You write the scanned-font editor and the one-click certify step; the rest is integration.

&#91;embedded content: Own app architecture · 6 steps, 2 new components\]

The order matters: encryption happens before certification, so the seal covers the final bytes. Signing uses your certificate, and the stamp's QR code sends a reader to a page that checks the signature cryptographically.

## The hard part: editing scanned text in the original font

No open-source equivalent of Acrobat's scanned-text editing exists, so this is the one feature you must build. The nearest attempt, [smoothscan](https://github.com/ncraun/smoothscan) (GPL-3.0), was abandoned in 2013. OCRmyPDF declined the feature in [issue #212](https://github.com/ocrmypdf/OCRmyPDF/issues/212). Every component it needs is available under a permissive licence.

Acrobat does not "find the font". It builds a new font out of the glyphs on the scan itself, then re-sets the page as real text in that font. An open reimplementation has two tiers:

**Tier 1, font matching (2–3 weeks).** Find the closest installed or Google font and edit in that.

1. OCR the page with word and character boxes (Tesseract, or PaddleOCR / RapidOCR for higher accuracy).
2. For each text run, render candidate fonts at the measured size and score them against the scanned glyphs, or use a font-ID model. Size, weight and slant come straight from the OCR boxes.
3. Erase the old words from the image (inpaint with the background colour) and place live text in the matched font. Keep the original image as an undo layer.

This gets 80–90% of the visual result for typed office documents, which use a small set of common fonts.

**Tier 2, a synthesised document font (6–10 weeks).** Build the scan's own font, as Acrobat does.

1. Cluster identical glyph shapes across the page or document with [jbig2enc](https://github.com/agl/jbig2enc) (Apache-2.0); Tesseract labels each cluster with its character.
2. Average each cluster into one clean bitmap and trace it to curves with libautotrace (LGPL-2.1). Potrace traces better but is GPL.
3. Assemble the curves into a TrueType font with fontTools (MIT), with advance widths and kerning measured from the scan.
4. Re-set the page as text in that font. Characters you type that never appear on the scan fall back to the Tier 1 matched font.

**Risks to design for**

- Clustering can merge look-alike glyphs (6/8, l/1), the same class of error that made JBIG2 notorious. Low-confidence words stay as image.
- Re-setting a page is irreversible, so always keep the original image.
- Font-ID models are trained on synthetic renders, and none of the good ones is licensed for commercial use. [mixfont/lens](https://github.com/mixfont/lens) is non-commercial, and font-classify has no licence. Retrain a DeepFont-style model on Google Fonts if you will distribute the app.

**Born-digital PDFs (not scans)** have a related problem. The embedded font is usually a subset, so letters you type may be missing. Reuse the subset when it has the glyphs; otherwise embed the matched full font and re-subset on save with fontTools or PDFium's new HarfBuzz subsetter.

## Signing, the verification box, and locking

Sign and lock in one step, in a fixed order: encrypt first, then certify-sign. Encrypting after signing rewrites the file's bytes and invalidates the signature ([Adobe](https://help.adobe.com/en_US/livecycle/10.0/Services/WS92d06802c76abadb-6ec569c512dbeb3d9d6-7ff5.html)).

**The "Sign & Lock" pipeline**

1. Flatten all edits, images, text boxes and drawn signatures into page content.
2. Optionally encrypt with AES-256 and permissions (no editing, signing allowed) using pikepdf or qpdf.
3. Let the user pick a certificate from the Windows store (CNG or CryptoAPI `MY`), a .pfx, or a USB token.
4. Write a **certification** signature, DocMDP P=1 ("no changes allowed"), as an incremental update with pyHanko, jSignPdf or PDF4QT.
5. Add an RFC 3161 timestamp, and optionally LTV data (PAdES B-LT/LTA) so it still verifies after the certificate expires.
6. Re-open the file and verify it before saving.

**Two kinds of lock: use both**

|  | Certification signature (DocMDP P=1) | Encryption permissions |
| --- | --- | --- |
| What it does | Any later change shows as a broken signature in every reader | Readers refuse to edit or print |
| Can it be removed? | Yes, but removal is detectable | Yes, silently: qpdf strips it in one command |
| Proves who and when | Yes | No |

**The visible verification box.** The box itself proves nothing, because anyone can draw one. Proof is the cryptographic signature that Acrobat Reader and others check. Make the box point to that check:

- Text: signer name, certificate issuer, date and time, "Certified: no changes allowed", and the first 16 characters of the document's SHA-256.
- A QR code linking to a verification page. pyHanko's `QRStampStyle` draws it, and `TextStampStyle` fills in `%(signer)s` and `%(ts)s`.
- The verification page can be a self-hosted [EU DSS](https://github.com/esig/dss) validator (LGPL-2.1). The reader uploads the PDF and gets a signature report.
- A line reading "Open in Adobe Reader to validate the signature" covers readers without the QR.

**Your certificate.** Under the [ECT Act](https://www.saflii.org/za/journals/DEREBUS/2012/69.html), any personal certificate gives an ordinary electronic signature, and courts have accepted far weaker forms. It counts as an *advanced* electronic signature only if it comes from an SAAA-accredited process, such as [LAWtrust AeSign](https://www.lawtrust.co.za/advanced-electronic-signatures/). You need that where a law requires a signature without naming the type. Wills, sales of land and long leases are excluded entirely. Label the box "electronic signature" unless your certificate is accredited.

**Windows store gap.** pyHanko is the best signing library but cannot read the Windows certificate store. That needs a small custom `Signer` calling CNG (about a week), or use jSignPdf or PDF4QT, which already do.

## Contract packs: initials, several signature lines, splitting

All four contract needs can be built in about 5–6 weeks on top of the Sign & Lock work. The order is fixed: split, initial, sign each line, lock last. Anything changed after the first digital signature shows as a modification.

**The pack workflow**

1. **Split** the pack into separate contracts, before anything is signed. Splitting a signed file destroys its signatures.
2. **Initial** every page of each contract from your Initials slot.
3. **Find the signature lines.** The app searches the text, OCR'd if scanned, for "Signature", "Signed at", "Initial", runs of underscores, and any empty signature fields. It shows them as a checklist; you confirm, add or drag them.
4. **Sign** every line on the checklist.
5. **Lock.** The Lock button stays disabled until every required line has a signature, so a contract cannot be closed half-signed.

**Your four scanned signatures**

- Scan or photograph your signatures on white paper. The app crops each one and removes the background, either to a transparent 600-dpi PNG or traced to vector curves so it stays sharp at any size.
- Each one is saved to one of four named slots, for example Full signature, Initials, Short signature and Alternate.
- The library is stored in your Windows profile and encrypted with Windows DPAPI, so a copied file is useless on another PC.
- The image is only the visible mark. The proof is the certificate signature applied in steps 4–5.

**Initialling every page**

- "Initial all pages" places the Initials slot at a chosen corner and offset, on a page range, skipping pages that carry a full signature line.
- Positions can be saved as a template per contract type, so a repeat contract needs one click.
- Initials are drawn into the page content *before* the first digital signature, so the signature covers them.

**Signing several lines: two modes**

| Mode | What happens | PIN prompts | Use when |
| --- | --- | --- | --- |
| One signature, shown on every line (default) | Your signature image is placed on each line, then one certification signature seals the whole file | 1 | Contracts you sign alone, or sign last |
| One signature per line | Each line becomes its own signature field and is signed in turn. The last one locks the document using PDF 2.0 "lock on signing" (pyHanko `doc_mdp_update_value`) | 1 per line, unless your certificate caches the PIN | The contract already has signature fields, or each line must be verifiable on its own |

In per-line mode, "no changes allowed" must come with the *last* signature. If it came first, every later signature would break it.

If someone else signs after you, lock with "form filling and signing allowed" instead. Their signature then stays valid, and the last signer closes the document.

**Splitting a pack**

- Split by typed page ranges ("1–12, 13–20, 21–24"), by bookmarks, by blank or separator pages, or by detected titles. OCR finds headings such as "AGREEMENT" or "ADDENDUM" and proposes cut points in a thumbnail view for you to confirm.
- Each part is saved as its own file, named from its title, using qpdf (Apache-2.0), which splits without loss.
- The parts then run through initial → sign → lock as a batch, with one PIN for the batch where your certificate allows.

**Free today, partly**

- [PDFsam Basic](https://github.com/torakiki/pdfsam) (AGPL-3.0, free) splits by page ranges, bookmarks or size.
- [jSignPdf](https://github.com/intoolswetrust/jsignpdf) signs a batch of files and can append a further signature to an already-signed file, one line at a time.
- Stirling-PDF can stamp an image across a page range for initials (unverified on the desktop build).
- Missing from all free tools: the cleaned-up signature library, signature-line detection, the lock-only-when-complete checklist, and one-click pack processing.

**Effort:** signature library 1 week · initial all pages 1 week · line detection, multi-line signing and the lock rule 2–3 weeks · split and batch 1 week.

## Review, compress, export, share and compare

All eight of these are solved problems. PDF4QT already has highlights, comments and a document-compare tool, so the build is about 6–8 weeks of integration. Word export is the weakest link: no open-source converter matches Word's own.

| Feature | How the app does it | Free today | Component (licence) | Effort |
| --- | --- | --- | --- | --- |
| Highlight, remove highlight | Standard highlight annotations in any colour; click to delete, or "Remove all highlights" | Acrobat Reader, PDF-XChange free, PDF4QT, KillerPDF | PDF4QT annotations (MIT) | 1–2 days |
| Comments | Sticky notes, callouts and replies, a comments panel, and an exportable comment summary | Same as above | PDF4QT annotations (MIT) | 2–3 days for the summary |
| Save with both kept on top | Saved as annotations, not burned in, so they stay editable and removable. A "Flatten" option makes them permanent | Same as above | PDF4QT incremental save | Included |
| Reduce file size | Lossless clean-up (shared fonts and images, compressed structure), then optional image downsampling to 150–200 dpi, JPEG for colour pages and JBIG2 for black-and-white scans. Presets such as "Email" and "Archive" | OCRmyPDF `--optimize 3` (command line) | qpdf (Apache-2.0), jbig2enc (Apache-2.0); avoid Ghostscript (AGPL) | 1–2 weeks |
| Export to Word | Hand the file to Microsoft Word, which converts PDFs itself and gives the best layout. Built-in fallback: pdf2docx. Scans are OCR'd first | Microsoft Word (in Microsoft 365) | [pdf2docx](https://github.com/ArtifexSoftware/pdf2docx): MIT, unmaintained, built on PyMuPDF (AGPL) | 1–2 weeks |
| Export to Excel | Detect the tables, write one worksheet per table. Excel 365's Get Data → From PDF is the native alternative | Excel (in Microsoft 365) | pdfplumber or Camelot (MIT) for digital tables; [Docling](https://www.infoworld.com/article/3997240/docling-an-open-source-tool-kit-for-advanced-document-processing.html) (MIT) for scanned or complex tables; openpyxl (MIT) | 1–2 weeks |
| Share by email or WhatsApp | The Windows Share sheet lists Outlook, WhatsApp desktop and Teams where installed, via `ShowShareUIForWindow` ([Microsoft](https://learn.microsoft.com/en-us/windows/apps/develop/windows-integration/integrate-sharesheet-send)). An "Email" button opens an Outlook draft with the file attached | Explorer's right-click Share works for any file | Windows Share API (free) | 3–5 days |
| Compare two files | PDF4QT's DocDiff already compares two PDFs page by page and marks the changes. Add a contract redline mode: word-level differences that ignore page breaks, OCR for scans, export as a marked-up PDF | PDF4QT DocDiff | PDF4QT (MIT) | 2–3 weeks for redline mode |

**Two rules the app must enforce**

- **Compress before signing, never after.** Reducing size rewrites the file and breaks every signature. The Sign & Lock step offers "reduce size" first; on an already-signed file the button is disabled.
- **Choose the lock level for comments.** "No changes allowed" forbids even comments after signing. If reviewers must comment on a signed contract, lock with "comments allowed", which keeps the signature valid while notes are added.

**Sharing signed files.** Send the PDF as a document or attachment, never as a photo: a photo is recompressed and the signature is lost. Documents are generally reported to arrive unchanged; confirm once by comparing the file's SHA-256 hash before and after sending by WhatsApp, Gmail and Outlook. WhatsApp accepts documents of up to 2 GB; corporate email often caps attachments at around 20–25 MB, so the app warns and offers "reduce size" before signing.

## Android version

Build Android as a second, touch-screen front end on the same PDF4QT core. One signing engine then serves both platforms, so a contract signed on your phone validates exactly like one signed on your PC. This adds about 4–6 months and starts with a 2-week proof of concept, because nobody has built PDF4QT for Android before.

**No free Android app signs PDFs with a certificate today, which is the gap this fills.**

| App | Certificate signing on Android | Notes |
| --- | --- | --- |
| [Adobe Acrobat Reader](https://community.adobe.com/t5/acrobat-reader-mobile-discussions/certificate-digital-signature-on-ios-and-android/m-p/12937333) | No | Cannot sign or verify with a digital ID on mobile |
| [Foxit PDF Editor](https://help.foxit.com/manuals/pdf-editor/mobile/en_us/12.0.0/Add_Signatures.html) | Yes | Paid subscription only |
| [Xodo](https://xodo.com/blog/esignatures-available-for-xodo-mobile-apps), [PDFgear](https://www.pdfgear.com/sign-pdf/how-to-sign-a-pdf-on-android.htm) | No | Drawn or image signatures only |
| PDF-XChange | No | No Android app |
| Open-source ([MuPDF viewer](https://github.com/ArtifexSoftware/mupdf-android-viewer), [Librera](https://github.com/foobnix/LibreraReader), [Images-to-PDF](https://github.com/Swati4star/Images-to-PDF)) | No | Viewing, highlights, notes, split and compress, but no visible certificate signing |

**How it is built**

- **Shared core.** PDF4QT's core library builds on its own and needs only Qt modules that exist on Android. [Qt 6.10](https://doc.qt.io/qt-6.10/android.html) supports Android 10–16 and is LGPL there, which is fine when Qt is linked dynamically.
- **New touch UI.** PDF4QT's screens are desktop widgets, so the phone app gets a new Qt Quick (QML) interface. This is most of the work.
- **Your certificate.** First version: import your .pfx into the app's private storage, protected by an Android Keystore key. This reuses PDF4QT's existing OpenSSL signing unchanged. Later: certificates installed in Android itself, picked from the system list via [KeyChain](https://developer.android.com/reference/android/security/KeyChain), 2–3 more weeks.
- **OCR:** Tesseract, which PDF4QT already uses ([Tesseract4Android](https://github.com/adaptech-cz/Tesseract4Android), Apache-2.0, proves it runs). Google's ML Kit is an option but is closed-source.
- **Sharing:** the Android share sheet sends the PDF to WhatsApp, Gmail or Outlook as a document.

**What the phone app does, and what stays on the PC**

| On Android | PC only (for now) |
| --- | --- |
| View, highlight, comment, images and text boxes | Scanned-text editing in the original font |
| Signature library, initials on every page | Word and Excel export |
| Sign several lines, certify, lock, encrypt | Contract redline compare |
| Split, reduce size, share |  |

**Risks and fallback**

- Two parts of PDF4QT may not build for Android unchanged: its Blend2D graphics library and some parallel-processing code. The proof of concept settles this: build the core for a phone, sign a test file with a .pfx, and open the result in Adobe Reader on a PC.
- **Fallback if the proof of concept fails:** a separate native Android app on MuPDF, whose Java API takes a pluggable signer and draws visible signature boxes. It takes about 3–5 months, but the app must then be open-sourced under AGPL (or a MuPDF licence bought). You would also maintain two signing engines that must be cross-checked.
- **Avoid:** Flutter, whose PDF editing and signing needs commercial Syncfusion; .NET MAUI and Tauri are too weak for PDF signing.

## Licences: what each one means for you

For use only inside Carousel, every licence below is fine: copyleft obligations start when you *give the software to someone else*. Licence choice matters only if the app may later go to other companies or be sold. In that case, stay on the permissive and LGPL rows.

| Licence | Components | Internal use | If you distribute the app |
| --- | --- | --- | --- |
| MIT / BSD / Apache-2.0 | PDF4QT, pyHanko, PDFium, qpdf, PDFBox, Tesseract, PaddleOCR code, jbig2enc, fontTools, open-pdf-sign | Free | Free; keep the notices |
| MPL-2.0 / LGPL | jSignPdf, EU DSS, libautotrace, OCRmyPDF, pikepdf | Free | Free; share changes to those files or libraries only, not your app |
| GPL-2.0 / GPL-3.0 | KillerPDF, Potrace, Okular, smoothscan | Free | Your whole app must be released as GPL source |
| AGPL-3.0 | MuPDF / PyMuPDF, iText, BentoPDF | Free | As GPL, and also if offered over a network; or buy a commercial licence |
| Non-commercial / none | mixfont/lens, font-classify, Surya weights above a revenue cap | Check terms | Not usable |

In short: KillerPDF and MuPDF are fine for your own desktop but lock a product into open-sourcing. PDF4QT (relicensed to MIT in Apr 2025) plus pyHanko or jSignPdf keeps every option open.

## Plan and effort

The whole build takes about 12–13 months for one developer working with Claude, or about 8 with a second developer on Android. Contract signing on the PC (Phases 1–3) is usable after about 3 months, and on Android after about 10. You are fully covered by free tools from day one, so nothing waits on the build.

&#91;embedded content\]

The code lives in the vault as a git repository with a second remote. Windows installers are built on a GitHub Actions Windows runner, so no development happens on your PC.

**Open questions**

- [ ] Is the app only for you, or will others in the group get it? That decides whether a GPL base such as KillerPDF is acceptable.
- [ ] Is your personal certificate from an accredited provider such as LAWtrust? That decides whether the box can say "advanced electronic signature".
- [ ] Install PDF-XChange free and sign a test file, to settle whether its signing is watermark-free.
- [ ] Is US$79 for PDF-XChange Editor Plus acceptable as a stop-gap while Phases 3–4 are built?
- [ ] Is your certificate a .pfx file, or on a USB token or smart card? A .pfx works on Android from day one; a token needs extra work on a phone.
- [ ] Do you have Microsoft 365? If so, Word and Excel do the export better than any open-source converter.

## Sources

GitHub stars, licences and release dates were taken from the GitHub API on 17 Oct 2026. Items marked unverified in the text are still open.

**Products and pricing**

- [PDF-XChange comparison chart](https://www.pdf-xchange.com/pdf-xchange-products-comparison-chart) · [demo limits](https://www.pdf-xchange.com/product/pdf-xchange-editor/demo_limits) · [shop](https://www.pdf-xchange.com/shopgt/pdf-xchange-editor.htm) · [PRO shop](https://www.pdf-xchange.com/shopgt/pdf-xchange-pro.htm) · [V11 signing manual](https://help.pdf-xchange.com/pdfxe11/digital-ids-and-signing-documents.html) · [EULA 2026](https://downloads-11.pdf-xchange.com/PDFXLicense_2026.pdf) · [forum: scanned editing needs Plus](https://forum.pdf-xchange.com/viewtopic.php?p=195698)
- [Adobe Acrobat compare versions](https://www.adobe.com/acrobat/pricing/compare-versions.html) · [Acrobat Standard OCR KB](https://helpx.adobe.com/uk/acrobat/kb/is-ocr-or-text-recognition-available-in-acrobat-std-dc.html) · [Acrobat certificate signing](https://helpx.adobe.com/acrobat/desktop/e-sign-documents/fill-sign-documents/add-digital-sign.html)

**Open-source apps**

- [KillerPDF](https://github.com/SteveTheKiller/KillerPDF) · [PDF4QT](https://github.com/JakubMelka/PDF4QT) · [jSignPdf](https://github.com/intoolswetrust/jsignpdf) · [Stirling-PDF](https://github.com/Stirling-Tools/Stirling-PDF) and [its licensing](https://docs.stirlingpdf.com/Server/Licensing/Plans-and-Licences/) · [Open PDF Studio](https://github.com/OpenAEC-Foundation/open-pdf-studio) · [BentoPDF](https://github.com/alam00000/bentopdf) · [open-pdf-sign](https://github.com/open-pdf-sign/open-pdf-sign) · [Okular signatures](https://docs.kde.org/stable/en/okular/okular/signatures.html) · [LibreOffice sign existing PDF](https://help.libreoffice.org/latest/en-US/text/shared/01/signexistingpdf.html) · [REA](https://github.com/morluto/rea)

**Components**

- OCR: [OCRmyPDF](https://github.com/ocrmypdf/OCRmyPDF) · [PaddleOCR](https://www.paddleocr.ai/latest/index.html) · [RapidOcrNet](https://packages.nuget.org/packages/RapidOcrNet) · [Tesseract for Windows](https://github.com/UB-Mannheim/tesseract/wiki)
- Scanned-font editing: [smoothscan](https://github.com/ncraun/smoothscan) · [OCRmyPDF #212](https://github.com/ocrmypdf/OCRmyPDF/issues/212) · [jbig2enc](https://github.com/agl/jbig2enc) · [AutoTrace](https://github.com/autotrace/autotrace) · [Potrace](https://potrace.sourceforge.net/) · [mixfont/lens](https://github.com/mixfont/lens) · [font-classify](https://github.com/Storia-AI/font-classify) · [DeepFont reimplementation](https://github.com/twelfth-star/universal-font-recognition)
- PDF engines: [PDFium fpdf\_edit.h](https://pdfium.googlesource.com/pdfium/+/refs/heads/main/public/fpdf_edit.h) · [MuPDF](https://mupdf.com/releases) · [Apache PDFBox](https://pdfbox.apache.org/) · [qpdf](https://github.com/qpdf/qpdf) · [pikepdf](https://pypi.org/project/pikepdf)
- Signing: [pyHanko](https://github.com/MatthiasValvekens/pyHanko) · [pyHanko signing guide](https://docs.pyhanko.eu/en/latest/lib-guide/signing.html) · [pyHanko CLI plugins](https://pyhanko.readthedocs.io/en/latest/lib-guide/cli-plugins.html) · [EU DSS](https://github.com/esig/dss) · [Adobe: encrypt before signing](https://help.adobe.com/en_US/livecycle/10.0/Services/WS92d06802c76abadb-6ec569c512dbeb3d9d6-7ff5.html)

**Standards and law**

- [ISO 32000-2 free access](https://pdfa.org/sponsored-standards) · [PDF Association announcement](https://pdfa.org/announcing-no-cost-access-to-iso-32000-2-pdf-2-0/)
- [ECT Act e-signatures (De Rebus)](https://www.saflii.org/za/journals/DEREBUS/2012/69.html) · [LAWtrust AeSign](https://www.lawtrust.co.za/advanced-electronic-signatures/) · [Copyright Act s19B](https://www.acts.co.za/copyright-act-1978/19b_general_exceptions_regarding_protection_of_computer_programs.php) · [ENS on the Copyright Amendment Bill ruling](https://www.ensafrica.com/news/detail/12103/a-decade-in-the-making-constitutional-court-r) · [Cybercrimes Act commencement](https://source.acts.co.za/cybercrimes-act-2020/proc_r_42_proclamation_no__r__4.php)
