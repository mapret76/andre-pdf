# R0 — independent review of A1–A16: what was done with each finding

Reviews: [R0-A.md](R0-A.md) (signing and contracts, A1–A8) and [R0-B.md](R0-B.md)
(A9–A15 and the mobile app), both made on commit 0ac8a12 by reviewers with a fresh
context in their own worktree. Fixes are in the commit that adds this file; the fixes
themselves are reviewed again (R0 verification).

| Finding | Severity | Status | What changed |
| --- | --- | --- | --- |
| A-H1 signing an encrypted PDF crashed or failed | high | fixed | `PDFDocumentReader` never calls an empty password callback; `writeIncrementalUpdate` and the signer take the password of the original (`Parameters::openPassword`); the editor asks for it when the empty password does not open the file; `sign()` is guarded by try/catch in the plugin. Test: countersign an encrypted contract certified P=2, without and with the password. |
| A-M1 signature lines on rotated pages | medium | fixed (appearance: limited) | The line finder works in the displayed page and maps back. On a rotated page the contract flow draws every image into the page and makes the digital signature invisible instead of drawing a turned appearance. Test: line on a /Rotate 90 page. |
| A-M2 line finder misses SA phrasing | medium | fixed | Attestation clauses ("Signed at … on this … day of", Afrikaans) excluded; "Signature & Date", "Name and Signature", "For the Purchaser", "on behalf of", "Vir die Koper" accepted. Manual "add a line" remains open. |
| A-M3 pack flow signs every line by default | medium | fixed | No choice is preselected; the user must pick "every line" or "lines labelled with …"; the report names the lines signed in each contract. |
| A-M4 file fingerprint unverifiable after a rewrite | medium | fixed | The stamp carries the file fingerprint only when the signature is appended to the original bytes (not with encryption, not in the pack flow). |
| A-M5 other parties cannot sign an encrypted P=2 file | medium | fixed (Reader gate open) | Lines left to another party get empty signature fields (`PDFSignatureLineFinder::addSignatureFields`) in the contract and pack flows. To be confirmed in Acrobat Reader. |
| A-L1 `isOK()` ignored appended bytes | low | fixed | `SignedFileCheck::isCoveringWholeFile`; test with appended bytes and with a later countersignature. |
| A-L2 plain library accepted on Windows | low | fixed | On Windows only the DPAPI form is read. |
| A-L3 passwords not wiped | low | open | Upstream behaviour (QString); noted. |
| A-L4 contract flow half done on cancel | low | fixed | The document before the flow is restored when it does not end signed; "No changes" with lines left to others asks first. |
| A-L5 extracted part kept foreign fields | low | fixed | Fields without a widget on the part's pages are dropped; test. |
| A-L6 bidi controls in file names | low | fixed | `\p{Cf}` and DEL removed, base name sanitised, trailing dots removed; test. |
| A-L7 QR ECC low, CN unescaped, may throw | low | fixed | ECC medium, control/format characters removed from the signer name, encoding failure gives a stamp without QR. |
| A-L8 pack flow checked bytes in memory | low | fixed | The written file is read back from disk. |
| B-H1 mobile app aborted on password-protected PDFs | high | fixed | Reader guard (above); the mobile app asks for the password and reuses it to sign, check and reopen. |
| B-H2 OS/2 table 100 bytes | high | fixed | 96 bytes; test parses the table (version, char range, typo/win metrics, x-height) and checks the ascent. |
| B-H3 Excel numbers wrong (IDs, phones, decimals) | high | fixed | Only unambiguous numbers convert; leading zeros, 11+ digits, dates and one separator with three digits stay text; tests for every case reported. |
| B-M1 font installer trusted the catalogue | medium | fixed | Font IDs validated, file and registry names from the ID / safe characters only, HTTPS to api.fontsource.org and cdn.jsdelivr.net only, size limits; tests with a hostile catalogue (path in the ID, file:// URL). |
| B-M2 foreign OCR text kept the old words | medium | fixed | On a pure scan (images + invisible text) the invisible text in the area is removed and the page written again; otherwise the user is warned. Test with a scan and another program's OCR text. |
| B-M3 plugin migration re-enabled disabled plugins | medium | fixed | Without "KnownPlugins" every plugin counts as known except those Andre PDF added. |
| B-M4 Share / Word passed the unsaved file | medium | fixed | Share, Send by E-Mail and Open in Word offer to save first. |
| B-M5 U+FFFE/U+FFFF made .docx/.xlsx invalid | medium | fixed | Only XML 1.0 characters are written; test validates the files with Python. |
| B-M6 redline diff memory | medium | fixed | Only the live diagonals are kept per round, limit 4000 edits; test: two unrelated texts of 3000 words. |
| B-M7 font name parsing | medium | fixed | Known names before suffix stripping, style words only as suffixes; tests. |
| B-L1/L2/L3 font download limits and hosts | low | fixed | With B-M1. |
| B-L4 ZIP without zip64 | low | open | 65 535 entries / 4 GB are far above a document export. |
| B-L5 multi-column pages interleaved in exports | low | open | Known limit of the line regrouping; noted. |
| B-L6 mobile overwrote `_SIGNED.pdf` | low | fixed | Numbered names; the signed file is reopened. |
| B-L7 .pfx password kept in the field | low | fixed | Cleared after signing. |
| B-L8 no "open with" intent | low | open | Needs Android intent handling; after the first APK. |
| B-L9 scan font harvest allocates per word | low | open | Slow on big pages, not a leak. |
| B-L10 partial font install | low | fixed | Reports the styles installed. |
| B-L11 export name of an unsaved document | low | fixed | Documents folder, "Document". |
