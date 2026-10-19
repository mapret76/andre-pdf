# R2-B: the verification report (B2) and the R1 fixes

Reviewer: independent, fresh context. Commit `1f98cfd` (worktree `/home/andre/projects/andre-pdf-review`,
Debug core build `build-review`; `Pdf4QtLibGui` and the plugins are read only, they are not built here).

Scope: B2 (`pdfsignaturereport.{h,cpp}`, `SignaturePlugin::onVerificationReport`, the ECT Act text in the
report and in `docs/andre/USER-GUIDE.md`), and the fixes of [R1-fixes.md](R1-fixes.md) except N3 and A-M4.

Method. I read every hunk of `git diff 0ac9e0a 1f98cfd` in scope and the whole of the report code. Then
three things were run:

- the three test binaries (`UnitTestsLongTermValidation`, `UnitTestsUpdate`, `UnitTestsSignatureBuilder`);
- a temporary probe slot in `tst_longtermvalidationtest.cpp` (forged and foreign revocation data in a
  `/DSS`, a signature whose hole is larger than its `/Contents`, a decoy signer certificate), built, run and
  reverted again;
- nine dry runs of `tools/andre-release.sh` with a stub `gh`, a stub `git` that answers `fetch` with
  nothing and otherwise passes through, a throwaway P-256 key and a scratch `HOME` (the real key was never
  read; nothing contacted GitHub).

"Proven" below means a probe, a test or a dry run showed it; "reading" means by code reading only.

## 1. Verification of the R1 fixes

| Item | Claimed | Verified | Evidence, and what is left |
| --- | --- | --- | --- |
| N1 label rules | fixed | yes | `pdfsignaturelinefinder.cpp:55-83`: attestation only for `signed at/this`, `signed on` not followed by `behalf`, `day of`, `dag van`; `other` wins over a signature word unless `paired`; "Initials and Surname" is a name line. Every probe string of R0 is in `signatureLineLabels` (`tst_signaturebuildertest.cpp:1165-1192`), and the test passed. Two small edges: `paired` has no `\b` after `and`/`en` ("Signature endorsed by" counts as paired; harmless), and a colon is not a pairing character ("Signature: Name:" on one line is now a name line). |
| N2 per-item removal | fixed | yes | `pdfscantexteditor.cpp:198-248`: only the invisible text items whose box meets the area go; state updates and the other items are kept and the element is re-serialised by `createItemsAsText`, which writes absolute positions (`<tpos>` from the state's text matrix, `pdfpagecontenteditorprocessor.cpp:1590-1600`), so the later lines do not shift. Test row "one text object" checks "Elsewhere" and "Third line" at their places. A visible text item still aborts (`return nullptr`), as before. |
| N4 pack flow on rotated pages | fixed | yes (reading) | `signatureplugin.cpp:1154-1205`: on a rotated last page the image is drawn into the page and the signature field is invisible; the line count is taken before the pop. Not said anywhere: such a contract has no stamp and no QR code, and the pack report does not mention it (same as the contract flow). |
| N5 duplicate field names | fixed | yes (reading) | `pdfsignaturelinefinder.cpp:91-113`: fully qualified names of the form are collected and the counter skips them. |
| N6 DPAPI failure | fixed | yes (reading, Windows-only path) | `pdfsignaturelibrary.cpp:151-160,186-199`: `protect` returns nothing on Windows when `CryptProtectData` fails; `save` refuses with a message before anything is written. |
| N7 space-grouped codes | fixed | yes | `pdfofficeexporter.cpp:287-293`: leading zero and more than 10 digits checked on the normalised integer part. Test `officeExport` has "012 345", "62 123 456 789", "1 234 567 890 123", "0123.45", "011 234 5678"; passed. |
| N8 mobile exception safety | fixed | yes (reading) | `pdfmobilecontroller.cpp:295-315`: `try`/`catch (const std::exception&)`; `PDFException` derives from `std::exception` (`pdfexception.h:31`). |
| N9 `%4` in labels | fixed | yes (reading) | `signatureplugin.cpp:1139,1254-1258`: one multi-argument `arg()`; the remaining chain at `:1271` has only the output directory (the user's own path) before the report text. |
| B-L2 jsDelivr sees the IP | fixed | yes (reading) | `scantextdialog.cpp:331-333` and `THIRD-PARTY-NOTICES.md:64`. |
| B-L10 failed styles not shown | fixed | yes (reading) | `scantextdialog.cpp:346-350` warns with the message on a partial install; `pdffontinstaller.cpp:338` uses the multi-argument `arg()`. |
| H1 release script finds no MSI | fixed | yes (proven) | `andre-release.sh:53-56`. Dry run with the `gh` layout `artifact/<name>.msi/<name>.msi`: the script reaches `gh release create` with the MSI, the manifest and the `.sig`; with two MSI files it dies "the run has 2 MSI files". |
| M4 any branch, any version | fixed | yes (proven) | `:27-31` (`headBranch=="master"`, `git fetch`, `merge-base --is-ancestor`), `:39-44` (newer than latest), `:75-80` (summary shown, "yes" required). Dry runs: a non-master run dies; latest `v1.0.0` or `v1.2.0` dies "not newer"; an existing release dies; the `merge-base` check ran against the real local `origin/master`. |
| L9 script details | fixed | yes (proven) | Key mode 600 (dry run with 644 dies); notes regex `(?![\d.])` takes `## 1.0` for 1.0 and `## 1.0.0` for 1.0.0 (checked in Python); the manifest tool, the changelog and the public key come from `$sha` (`:48-50`); the draft clean-up is documented (`:12-13`). Key rotation left, as stated. |
| M1 Markdown with HTML | fixed | yes | `pdfversionhistory.cpp:120-140` escapes every ASCII punctuation and symbol character and turns line breaks into spaces; headings are built only from a validated version and date. Both dialogs render with `MarkdownNoHTML` (`pdfupdatedialog.cpp:51`, `pdfversionhistorydialog.cpp:49`); the update dialog has `setOpenLinks(false)`. Test `manifestRules` shows `<Enter>`, `<!--`, `*bold* [link]` and a fake heading as plain text; passed. |
| M2 TOCTOU before msiexec | mitigated | yes (reading) | `pdfupdatechecker.cpp:364-376` (random name, `NewOnly`, old installers removed), `:423-457` (`CreateFileW` with `FILE_SHARE_READ` only, hash again, handle kept). The handle is released when the program quits, which `offerUpdate` does right after `startInstaller`, so the protection ends milliseconds after msiexec starts; msiexec reads the package again after that, as the comment says. "Mitigated" is the honest word. Test: a changed file is refused by `startInstaller` (Linux path). |
| M3 no cancel, no deadline | fixed | yes | `:108-159` (cancel per chunk and polled every 200 ms, deadline 120 s + size at 64 kB/s), `pdfprogramcontroller.cpp:1771-1778` (Cancel button, atomic flag). Test: a cancelled download leaves no file; passed. Cosmetic: after the user cancels, the "critical" box says "The download was cancelled." |
| L1 redirects | fixed | yes (reading) | `:88-103`: `UserVerifiedRedirectPolicy`, HTTPS, no user info, allowed hosts. A port on the redirect target is not refused (the manifest URL check does refuse one); the SHA-256 pin makes this cosmetic. |
| L3 loose fields | fixed | yes | `pdfversionhistory.cpp:36` anchored; `pdfupdatechecker.cpp:313-317,323-331`. Tests for `"1.0.0\n"`, 2048.5, `zz`, user info, port; passed. |
| L4 Enter twice | fixed | yes (reading) | `pdfupdatedialog.cpp:58-59`, `pdfprogramcontroller.cpp:1805-1806`. |
| L5 quit asks twice | fixed | yes (reading) | `pdfprogramcontroller.cpp:1813-1821`: `m_mainWindow->close()` first (one save question, cancel stops the update), then `startInstaller`, then `quit()`. |
| L6 once a day | fixed | yes (reading) | `:1686-1688` (`==`), `:1718-1722` (date stored after a completed check). |
| L7/L8 threads | fixed | partial | `:1696-1700` one check at a time; the thread is deleted in `finished`. A check that is still running when the program quits is not joined (`aboutToQuit`), so the shutdown race of L7 remains for up to the two transfer deadlines. |
| L10 old MSIs | fixed | yes | `:364-368`; test checks the first file is gone after the second download; passed. |
| L11 parsers disagree | fixed | yes | `pdfversionhistory.cpp:64-71` and `make_update_manifest.py:12-14`; test with `## 1.2.3.4.5`; passed. |
| L12 other platforms | fixed (Linux) | yes (reading) | `pdfprogramcontroller.cpp:1686-1688`: no automatic check unless `getPlatform() == "windows"`. |
| L13 warning / tests | partly | yes | `tst_updatetest.cpp:328` checks `open()`. The dialogs still have no test. |
| L14 P-256 only | fixed | yes (reading) | `pdfupdatechecker.cpp:230-234` (`EVP_PKEY_get_group_name == prime256v1`). The test hook stays, as accepted. |

The Windows-only code of `startInstaller` (`CreateFileW`, `ReadFile`, `msiexec` from `%SystemRoot%\System32`
with an argument list) is read only: the calls and types are right for Qt 6.10 and Win32; it cannot be
compiled or run here.

## 2. Findings on B2, the verification report

### Medium

**B2-M1. "The signature covers the whole file" is computed from the last byte of the byte ranges, not
from what the ranges cover** (proven)
`Pdf4QtLibCore/sources/pdfsignaturereport.cpp:414-418` (`signedBytesEnd = max(offset+size)`,
`isCoveringWholeFile = signedBytesEnd == data.size()`), `:554-560` (the Integrity row).
- The verifier already computes the real coverage (`PDFSignatureVerificationResult::getBytesCoveredBySignature`,
  `pdfsignaturehandler.cpp:708-771`), and only warns when bytes inside the signed span are not covered;
  `isSignatureValid()` stays true. The report does not use it.
- Probe B: a signature whose first range stops 12 bytes before the `<` of `/Contents` (the signer chooses
  the `/ByteRange`; a hostile signer can leave any hole, up to a hole over page content). The report says
  **"VALID: the signed bytes have not been changed since signing; the signature covers the whole file"**,
  and three rows below, **"Warning: 12 bytes are not covered by signature."** Two statements that cannot both
  be true, in the one line a third party reads.
- Scenario: Mallory signs a contract with a hole over the clause with the price, then changes the price in
  the hole. Acrobat shows the document as modified; the Andre PDF report shows VALID and "covers the whole
  file", with a warning the reader has been told to ignore ("only signatures, timestamps and validation data
  should be among them" refers to appended bytes, not to holes).
- Fix: take the interval set of the result: `covering = set.isCovered(0, size-1)`; `appended` = bytes after
  the last covered byte; `uncoveredInside` = size - appended - set.getTotalLength(). If `uncoveredInside > 0`,
  the Integrity row must say "INVALID as a whole-document signature: N bytes inside the signed part are
  not covered" and the PAdES level must be empty. Add this case to `verificationReport`.

**B2-M2. The signer's identity is taken from a certificate the signature does not bind: a decoy
certificate with another name passes as the signer** (proven)
`pdfsignaturereport.cpp:110-133` (`addCertificates`: first certificate for which `CMS_SignerInfo_cert_cmp`
matches, i.e. issuer and serial number), `:433-443` (chain), `:466-477` (PAdES level), `:656-670` (ECT text).
- CMS binds the signer certificate through the signed attribute `signingCertificateV2` (ESS, RFC 5035), which
  carries the SHA-256 of the certificate. Neither the report nor the upstream verifier checks it
  (`pdfsignaturehandler.cpp:591` also takes the first certificate with that issuer and serial). The
  certificate set of a CMS is not signed, so anyone can edit it after the fact.
- Probe C: the real certificate of the test CA (CN "Andre PDF test signer", serial 0A) was copied, its subject
  changed to "Andre Pretorius", signed with a random key (the public key inside stays the signer's), and
  placed first in the certificate set of an otherwise untouched signature. The report shows: Integrity
  VALID; Format "PAdES baseline B-B"; "Certificate of the signer: Name Andre Pretorius, serial 0A,
  SHA-256 F9:DD:..." (the real certificate is 36:3F:...); "Certificate trust: NOT trusted by this computer
  (Error)"; "The chain is incomplete"; and the ECT paragraph: "This is an advanced electronic signature ...
  only if the certificate was issued under a process accredited ... The issuer is "CN=Andre PDF test root CA":
  compare it with the Authority's register".
- Scenario: Mallory holds a real certificate (any CA) and signs. She rewrites the certificate set so the
  report prints Andre's name with the real issuer's name next to it. The stamp and QR code (also made by the
  signer) carry the decoy's fingerprint, so the report's own step 2 ("must equal the fingerprint in the
  stamp") passes. Only a call to the issuer about serial 0A would reveal it. Andre's own self-signed
  signatures show "NOT trusted" too, so the trust row does not stand out.
- Fix (small): after `addCertificates`, verify the ESS attribute against the chosen certificate:
  `OSSL_ESS_check_signing_certs(ss, ssv2, chain, 1)` with `ss`/`ssv2` read from the signed attributes
  (`CMS_signed_get0_data_by_OBJ`), or compare the SHA-256 of the certificate with the first `ESSCertIDv2`.
  On a mismatch: an Error row "The certificate in the signature is not the one the signer committed to
  (ESS signing certificate mismatch)", `isIntegrityValid = false` for the report, no PAdES level. A
  signature without the attribute is not PAdES (already so) and should say that its certificate is unbound.
  Add the decoy case to the tests (the probe code is in this review's history: `CMS_add1_cert` of the
  decoy; DER sorts the SET OF, so the decoy lands first by itself).

**B2-M3. The ECT Act paragraph keys on the issuer's name and ignores whether the chain verified**
(reading, shown by probe C)
`pdfsignaturereport.cpp:656-670`.
- The paragraph is printed for every signature whose certificate is not self-signed, and tells the reader
  to look the issuer's name up in the register. A self-made CA named "LAWtrust AeSign CA", or the decoy of
  B2-M2, gets the same paragraph as a genuine accredited certificate; the only hint is the separate trust
  row. Accreditation is a property of a verified chain, never of a name.
- Fix: three branches. Chain verified to a trusted root (`isCertificateTrusted`): "The issuer is X; it is an
  advanced electronic signature only if X is an accredited authentication service provider (register)".
  Chain not verified: "The issuer named in the certificate ("X") could not be verified; nothing about the
  issuer can be relied on; treat it as an electronic signature at most". Self-signed: as now. Say the same in
  USER-GUIDE.md 4a step 3.

### Low

- **B2-L1. PAdES level overstated** (reading). `:466-477`: "B-T" needs only a non-empty `timeStampToken`
  attribute, even when the verifier could not verify it (`timestampTime` invalid, warning "Timestamp ...
  could not be verified"); "B-LT" needs revocation data for the signer's certificate only, not for the
  issuers or the timestamp authority (ETSI EN 319 142-1 requires the full chains); "B-LTA" accepts a document
  timestamp whose own validation data is absent. Fix: B-T only with `timestampTime.isValid()`; B-LT only when
  every non-root certificate of the chain and of the token has revocation data in the file, otherwise say
  "B-T with partial validation data".
- **B2-L2. Revocation time is not related to the signing time** (reading). `:237-307` accepts any
  issuer-signed answer, whenever produced; the row prints the time but not whether it is before the
  signature. A "good" answer from before the signing says nothing about the status at signing, and the
  stale-answer rules (`nextUpdate`) are not applied. Fix: print "produced before the signature: it does not
  prove the status at signing" when `revocationTime < timestampTime` (or `claimedTime`), and note an expired
  `nextUpdate`.
- **B2-L3. No `try`/`catch` around the report in the plugin** (reading).
  `Pdf4QtEditorPlugins/SignaturePlugin/signatureplugin.cpp:996-999`: `PDFSignatureReport::create` runs
  `PDFForm::parse`, the DSS parse and the verifier on a file from disk without a guard; the signing paths of
  the same plugin have one. A `PDFException` from a hostile form ends the process. Fix: the same
  `try`/`catch (const std::exception&)` with a message.
- **B2-L4. Format characters in names are not removed** (reading). `:167-176,196-233`: `commonName`,
  `email` and the policy OIDs go to the HTML as they are; the stamp strips `Cf` and control characters
  (`pdfsignaturestamp.cpp:66`), the report does not, so a right-to-left override in a CN reverses the
  displayed name in the browser and in the saved PDF. HTML escaping itself is complete (every value passes
  `row()` or `escape()`, both `arg()` forms are safe, and the hostile-name test proves `<b>` is escaped).
  Fix: the same `clean` lambda in `describe()`.
- **B2-L5. Self-signed is decided by names, not by the signature** (reading). `:221` uses `EXFLAG_SS`, which
  OpenSSL sets from subject == issuer and matching key identifiers without verifying the signature. A
  "root" with a garbage signature reads "Self-signed: yes (root)". Fix: `X509_verify(cert, X509_get0_pubkey(cert)) == 1`.
- **B2-L6. Smaller points** (reading). Two fields with one fully qualified name both get the last result
  (`:420-431`); the password prompt reused for the report says "needed to add the signature to it"
  (`signatureplugin.cpp:410`); "Save as PDF" reports nothing when the file cannot be written (`:1019-1023`);
  the report runs on the GUI thread with a wait cursor (a file with hundreds of signatures blocks the
  window).

Checked and found sound (proven unless noted): revocation data in the `/DSS` counts only when the
issuer, or a responder it delegated, signed it: a genuine answer for another serial number, a "good" answer
signed by a responder with the issuer's name and another key, and a CRL with the issuer's name and another
key all give "no revocation data embedded in the file" (probe A1-A5; `OCSP_basic_verify` with only the
issuer as trust anchor, `X509_CRL_verify` with the issuer's key). Chain building accepts an issuer only when
it verifies the certificate's signature (`findIssuer`, `:87-98`), is capped at depth 10 and stops at a
self-signed certificate, so cycles and long chains end. Malformed CMS, missing signer, missing certificates
and a non-PDF all return a report without crashing (tests `verificationReport*`, and the probes); every
OpenSSL object in `create()`/`describe()`/`getRevocation()` is freed on every path (reading). The upstream
verifier and the report pick the same certificate (first match by issuer and serial), so the trust row and
the identity rows describe the same certificate (B2-M2 is about that certificate being unverified, not
about a mismatch between the two). HTML: every file and certificate value is escaped; the browser has
`setOpenLinks(false)`; the saved HTML carries `<meta charset="utf-8">`.

### The ECT Act text

Report (`:659-670,686-691`) and USER-GUIDE.md ("Legal note", section 4a).

- s13(1), s13(2), s13(3), s13(4): the paraphrases match the Act (where a law requires a signature and does
  not name the type, only an AES satisfies it; electronic form alone does not remove legal force; the
  identify-and-approve-and-reliable test where the parties require an electronic signature without agreeing
  a type; an AES is presumed valid unless the contrary is proved).
- s37-38 and "the Authority": right sections (accreditation of authentication products and services; the
  criteria). The Act establishes no "register" by that name (the Authority is the Director-General, s34);
  say "the Authority's published list of accredited authentication service providers".
- s4(4) and Schedule 2: the four transactions are listed correctly. The guide's "sign those on paper" is
  stronger than the Act ("must not be construed as giving validity"), softened by "unless another law
  provides otherwise"; acceptable.
- **Overstated (low):** an AES is a signature "which results from a process which has been accredited"
  (s1). The report says it is an AES "only if the certificate was issued under a process accredited", and the
  guide says "Andre PDF signs with such a certificate like with any other", which together suggest that a
  certificate from an accredited provider, used in Andre PDF, yields an AES. Whether signing in other software
  with the provider's certificate is still inside the accredited process depends on the scope of that
  provider's accreditation. Say: "may be an advanced electronic signature if the whole signing process,
  not only the certificate, is the provider's accredited one; ask the provider". With B2-M3, this is the
  only legal wording that needs a change.

## 3. Test results

| Run | Result | File |
| --- | --- | --- |
| `UnitTestsLongTermValidation` (clean tree, before and after the probe) | 13 passed, 0 failed | `out-r2b-ltv.txt` |
| `UnitTestsUpdate` | 7 passed, 0 failed | `out-r2b-update.txt` |
| `UnitTestsSignatureBuilder` (includes `signatureLineLabels`, `officeExport`, `scanTextForeignOcr`, `versionHistory`) | 66 passed, 0 failed, 35.9 s | `out-r2b-signaturebuilder.txt` |
| Probe `r2bProbe` (temporary slot, reverted) | A1 legit OCSP: Good; A2 OCSP of serial 14 for signer 0A: not checked; A3 forged "good" OCSP: not checked; A4 forged CRL: not checked; A5 both: not checked. B: integrity VALID, "covers the whole file", warning "12 bytes are not covered by signature". C: decoy fits (6164 of 6452 hex digits), integrity VALID, trusted false ("Error"), PAdES B-B, chain of 1 named "Andre Pretorius", serial 0A, decoy fingerprint; the ECT paragraph names the real root CA. | (console, in this report) |
| Release script dry runs (stub `gh`/`git`, throwaway key, scratch `HOME`) | pure: dies "the signature does not verify with the public key of the program" (fails closed); with the throwaway public key: reaches `gh release create` with MSI, manifest, `.sig`, the right notes heading and the summary; no latest release: same; non-master run: dies; latest v1.0.0 / v1.2.0: dies "not newer"; existing release: dies; two MSI files: dies; key mode 644: dies. | (console) |

All temporary source changes were reverted (`git status` shows only this report and the untracked
`out-r2b-*.txt`); `scratch-r2b/` was deleted; the LTV test binary was rebuilt from the clean source.
