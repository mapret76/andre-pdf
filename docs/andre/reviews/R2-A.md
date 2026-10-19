# R2-A - independent review of B1 (PAdES / long-term validation), B3 (stamp) and the signer changes

Reviewer: fresh context, not the author. Commit under review: `1f98cfd` (with `d02d69f`).
Scope: `pdfcertificatemanager.{h,cpp}`, `pdflongtermvalidation.{h,cpp}`, `pdfdocumentsigner.{h,cpp}`,
`pdfsignaturestamp.{h,cpp}`, the SignaturePlugin / sign dialog / mobile `sign()` integration
(read only, GUI not built here), the four tests, and the claims of `docs/andre/USER-GUIDE.md`.
Method: reading of every file in scope; the in-scope test binaries run from the Debug tree; two
suspicions proven at runtime with a temporary test added to `tst_longtermvalidationtest.cpp`
(reverted afterwards, the binary rebuilt from the clean source).

"proven" = reproduced by running code; "reading" = established from the source alone.

## Findings

### High

None. Nothing in scope lets an attacker break a signature, alter signed bytes, or make the
program embed data it did not verify against the issuer. The signed bytes are never touched
(`addValidationData` refuses a result that does not start with them, proven by
`validationDataFromOcsp` and `extensionWithoutTimestampAuthority`).

### Medium

**M1 - Stale revocation data is embedded silently; no freshness check against the signing time**
(`pdflongtermvalidation.cpp:327-328`, `:398-402`) - proven.
`OCSP_check_validity(thisUpdate, nextUpdate, 300, -1)` with `maxsec = -1` accepts any response
that has no `nextUpdate`, however old; `getCrl` checks the issuer's signature only and never
`nextUpdate`. With the loopback CA switched to a `thisUpdate` 30 days old without `nextUpdate`,
and to a CRL whose `nextUpdate` lay 30 days in the past, `addValidationData` returned
`ocspCount 1` / `crlCount 1` with an empty `warnings` list. Independently of staleness, nothing
compares the response's `producedAt`/`thisUpdate` with the time of the signature timestamp: an
OCSP answer produced *before* the signature cannot attest the certificate's status *at* signing,
and PAdES validators (EU DSS, Acrobat's "LTV enabled" logic) treat such data as not fresh and
return INDETERMINATE rather than B-LT.
Scenario: a CA's CDN serves a cached "good" response without `nextUpdate` from last week; the
certificate was revoked yesterday; the contract carries a "good" status that no validator will
honour, and the program told the user "long-term validation data embedded".
Fix: pass a positive `maxsec` (for example 7 days) and refuse an OCSP response without
`nextUpdate` older than that; reject a CRL whose `nextUpdate` has passed (`X509_cmp_current_time`);
after the signature timestamp is known, require `thisUpdate` (OCSP) / `thisUpdate` (CRL) to be
later than the timestamp's time, else re-request with a nonce and warn if still older. Add the
two cases to the test (the scratch version of the test is described above).

**M2 - The program's own verifier ignores the OCSP answers it embeds and warns on every
long-term file** (`pdfsignaturehandler.cpp:1160-1172`, `:1200`, `:1050-1057`, `:765-768`) - reading.
Switching the default subfilter to `ETSI.CAdES.detached` routes every Andre PDF signature through
`verifyCertificateCAdES`, which sets `X509_V_FLAG_CRL_CHECK | CRL_CHECK_ALL`, loads only the
DSS *CRLs* into the store and never reads the DSS *OCSPs*. The default LTV path stores OCSP first
and a CRL only on fallback, so the typical file has no CRL: OpenSSL reports
`X509_V_ERR_UNABLE_TO_GET_CRL`, which the callback downgrades to the warning "unable to get
CRL". Together with the pre-existing "N bytes are not covered by signature" warning every
B-LT/B-LTA file the program produces shows a yellow (or, with "treat warnings as errors", red)
icon in the program's own signature panel, while Acrobat shows green. `checkSignedFile` looks at
`Signature_OK` and coverage only, so the "read back and checked" dialog does not notice.
Fix: feed the DSS OCSP responses to the verifier (`X509_STORE_CTX` with an OCSP check, or at
least treat a stored OCSP "good" for the leaf as satisfying the CRL flag), and suppress the
coverage warning when the uncovered tail is exactly DSS and DocTimeStamp updates (the report code
already describes it that way). Until then the user guide's "shows what a reader will see" is
not what the program itself shows.

**M3 - The stamp text truncates the serial number and the issuer name without saying so**
(`pdfsignaturestamp.cpp:153`) - proven.
`tr("Issued by %1, serial %2").arg(issuerName.left(40), serialNumber.left(24))`. Real CA serial
numbers are 16-20 bytes = 32-40 hex digits (LAWtrust, DigiCert, Sectigo all use random 16+ byte
serials). With a 20-byte serial the stamp prints
`Issued by LAWtrust Advanced Electronic Signature C, serial 0123456789ABCDEF01234567` - 24 of
40 digits, no ellipsis, the issuer cut mid-word. B3's purpose is that a third party compares the
printed serial with Acrobat's certificate details; a truncated serial will not match (or, worse,
matches the prefix of a different certificate of the same CA). The QR payload carries the full
value.
Fix: never truncate identifiers - wrap the serial on two lines (the stamp already computes its
height from the line count), group it in fours like the fingerprint, or print the last 16 digits
with a leading "…" so the cut is visible. Elide the issuer name with "…".

**M4 - "PAdES B-LTA" is claimed by the saved-file dialog when it is not true**
(`pdflongtermvalidation.cpp:701-702`; `signatureplugin.cpp` `checkSavedFile`, pack report) - reading.
`Extension::getReport()` prints "document timestamp added over the signature and its validation
data (PAdES B-LTA)" whenever the timestamp was added, also when `validation.isOK()` is false (the
line above then says "NOT embedded") and also when the signer's certificate is self-signed - the
default certificate the Certificates Manager creates - for which no revocation data can exist and
the program's own report (`pdfsignaturereport.cpp:468-492`) correctly stops at "B-T". The user is
told B-LTA, the verification report says B-T.
Fix: print the B-LTA line only when `validation.isOK()`, no certificate of the signer's chain is
in `warnings`, and the signer is not self-signed; otherwise "document timestamp added (PAdES B-T
with a document timestamp)".

**M5 - The production signing path for an accredited certificate is untested, and no level has
been checked by an external validator** (`pdfcertificatemanager.cpp:801-920`) - reading.
`signImpl_Win` (Windows store / smart card, the only way to use a LAWtrust token) now builds the
chain with `CertGetCertificateChain` and hand-encodes the `signing-certificate-v2` attribute
(`1.2.840.113549.1.9.16.2.47`, DER of `ESS_SIGNING_CERT_V2` with issuerSerial) for
`CryptSignMessage`. Every test signs with a PKCS#12 entry, which takes the OpenSSL path on
Windows too, so this code has never produced a signature anywhere. By reading it is plausible
(same DER as the store certificate, `rgAuthAttr` makes CryptoAPI add content-type and
message-digest, no signing-time, chain contexts freed after use), but a wrong attribute encoding
means a signature Acrobat accepts and every CAdES validator rejects, and a `CryptSignMessage`
failure means no signature at all - exactly on the path the legal note sends the user down.
Nothing produced by either path has been opened in Acrobat Reader or run through a PAdES
validator (EU DSS demo, `etsi-conformance`), and the interplay DocMDP P=1 + DSS + DocTimeStamp
(allowed by ISO 32000-2 12.8.4 / PAdES, and what the sign flow produces by default for a
"No changes" lock) has not been seen in Acrobat.
Fix before B1 is ticked: on the Windows CI runner import the test certificate into the user's
`MY` store and sign through `signImpl_Win`; dump the CMS and check signed attributes (content-type,
message-digest, signing-certificate-v2, no signing-time) and that `CMS_verify` / `PKCS7_verify`
accepts it; open one B-LTA file with a P=1 lock in Acrobat Reader and in the EU DSS validator and
record the result in the ledger.

**M6 - Network requests driven by any certificate in the document: no cancel, no bound on total
time, redirects to any host, nested event loop on the GUI thread**
(`pdflongtermvalidation.cpp:161-217`, `:264-285`, `:297-375`, `:378-417`; `signatureplugin.cpp`
`extendSignature`) - reading.
The URLs come from the AIA / CRL distribution points of every certificate the pool collects: the
signer's chain, the TSA's chain, the responder certificates returned by any OCSP answer, and the
chains of signatures *other parties* put into the document earlier. Each URL gets a synchronous
GET or POST with a 15 s deadline; `NoLessSafeRedirectPolicy` follows up to Qt's default 50
redirects to any host; the scheme is limited to http(s) but not the destination (loopback, RFC
1918, link-local metadata addresses are reachable). The pool grows while it is walked: a
responder may return up to 1 MB of certificates, each with its own URLs. Nothing lets the user
cancel, and `request()` spins a `QEventLoop` on the GUI thread while the wait cursor is shown, so
menus and the sign action stay clickable (re-entrancy into `onSignDigitally`).
Scenario (DoS): a counter-party's certificate chain names 20 unreachable OCSP and CRL URLs; the
contract flow blocks for 20 x 15 s per certificate with no way out. Scenario (SSRF): a crafted
chain makes the signer's PC POST to `http://10.0.0.1/...` - blind, low value, but free.
Fix: one overall budget (for example 60 s) and a cancel button via `PDFProgress`; cap the pool
size and the number of URLs per certificate; refuse redirects off the original host (the update
checker already does this: "redirects held to the allowed hosts"); do not add responder
certificates to the pool beyond the one that signed the answer; consider running `extend` off the
GUI thread.

### Low

**L1 - An existing file cannot be re-extended** (`signatureplugin.cpp`, sign dialog) - reading.
B-LTA must be renewed before the TSA certificate expires, and the TSA chain of the *document*
timestamp needs its own DSS entry at the next extension. The only later action is "Document
timestamp only", which never calls `addValidationData`, so the second DSS is never written and
the LTA chain cannot be maintained. Fix: a "Add Validation Data and Timestamp..." action on an
opened signed file that calls `PDFLongTermValidation::extend` on the saved bytes.

**L2 - "CA issuers" answers in PKCS#7 form are not parsed** (`:264-285`) - reading.
Many CAs (Microsoft CA, several Sectigo/DigiCert intermediates) serve `.p7c` bundles at the AIA
URL; `d2i_X509` / PEM fail, the issuer is reported "not found" and the chain in the DSS is
incomplete. Fix: try `d2i_PKCS7` and take its certificates.

**L3 - Certificates already in the DSS are not seeded into the pool** (`:447-458`) - reading.
On a second extension the issuers present in `/DSS /Certs` but not in a CMS are re-fetched or
warned about. Fix: `pool.add` every `existing->Cert` before the walk.

**L4 - CRL semantics** (`:398-402`) - reading. A delta CRL or a partitioned CRL (issuing
distribution point) is accepted as a full CRL for the certificate; `X509_CRL_get0_by_cert`
returning 2 (reason `removeFromCRL`) counts as Good (correct for deltas, wrong for a full CRL).
Also `OCSP_CERTID_dup(id)` leaks if `OCSP_request_add0_id` fails (`:303`; cannot happen in
practice). Fix: reject CRLs with a delta or IDP extension, or use `X509_V_FLAG_CRL_CHECK` on a
store instead of the manual lookup.

**L5 - Signing fails outright when the TSA is unreachable** (`signatureplugin.cpp` sign switch,
`signWithTimestamp`) - reading. With "Signature with timestamp" preset, an offline user gets an
error and must know to switch to "Signature". The guide says "it needs the internet" but not
that signing is refused. Fix: offer "sign without timestamp (B-B)" in the error dialog.

**L6 - `checkExtendedFile` without a timestamp verifies nothing after the signed bytes**
(`:720-734`) - reading. The tail is only parsed for readability; the dialog nevertheless states
"only its long-term validation data follows it". `isCoveringWholeFile` is copied from the
in-memory check, so it reads true while the file has a tail. Fix: in the no-timestamp branch
parse the tail's catalog and require that the only changed objects are `/DSS` and its streams.

**L7 - User guide and ledger** (`docs/andre/USER-GUIDE.md` 4, 4a, Legal note; `ANDRE-PDF.md:124`)
- reading.
- "the European and South African practice for signed PDFs": no South African instrument names
  PAdES; say "the European standard (ETSI EN 319 142-1), accepted by Acrobat and the SA
  accredited providers".
- Guide cites "s4(4)" for Schedule 2, the ledger's decision row cites "s4(3)". One is wrong;
  check the Act's text (s4(3) concerns Schedule 1 laws, s4(4) the Schedule 2 transactions, to
  this reviewer's recollection) and align both.
- "import it in the Certificates Manager (or use it from the Windows store / its smart card)":
  an accredited certificate's key lives on a token and is not importable; only the Windows store
  path applies - the untested one (M5).
- Section 4 describes timestamp + LTV defaults; the Android section should say that the app
  signs B-B only (no timestamp, no LTV, `pdfmobilecontroller.cpp:288-306`).
- "After saving, the program reads the file back and shows what a reader will see": see M2.
- The stamp line "Issued by X" is the signer's own claim; for a self-signed certificate X is
  whatever the signer typed. The guide should say the issuer line proves nothing by itself and the
  fingerprint comparison is the check.

**L8 - Silent degradation on an older OpenSSL** (`pdfcertificatemanager.cpp:339-344`) - reading.
Both `CMS_CADES` and `CMS_NO_SIGNING_TIME` are under `#ifdef`; a build against OpenSSL < 3.2 emits
signing-time and no ESS attribute, and the report then says "not a PAdES baseline signature" for
the program's own output. Fix: `#error` instead, or `static_assert` on `OPENSSL_VERSION_NUMBER`.

**L9 - The "No changes" refusal trusts `/Perms /DocMDP` without a signature** (`pdfdocumentsigner.cpp:91-95`,
`pdfocrapplyprocessor.cpp:76-106`) - reading. Any PDF with a `/Perms << /DocMDP <ref> >>` whose
reference is an empty or broken signature dictionary with `P 1` refuses every signature in Andre
PDF (trivial denial of service; Acrobat would ignore a certification whose signature is invalid).
Fix: refuse only when the referenced signature field holds a non-empty `/Contents`.

**L10 - No system proxy for OCSP / CRL / CA-issuers / TSA** - reading. Only the OCR model manager
calls `QNetworkProxyFactory::setUseSystemConfiguration(true)` (`pdfocrmodelmanager.cpp:1705`).
Behind a corporate proxy every LTV request fails with a warning. Fix: set it once at application
start.

**L11 - A 32 MB CRL is embedded into every contract** (`MAXIMAL_CRL_SIZE`) - reading. Some
government CRLs are 20+ MB; OCSP-first mitigates. Consider 8 MB and a warning.

**L12 - Unicode line separators pass `clean()`** (`pdfsignaturestamp.cpp:59`, `:66`) - reading.
Only `Other_Control` and `Other_Format` are removed; U+2028/U+2029 (`Separator_Line/Paragraph`)
survive and a phone's QR reader shows them as line breaks. The signer controls their own name
anyway, so the value is nil; strip `QChar::Separator_Line` and `Separator_Paragraph` too.

### Checked and found sound

- OpenSSL ownership in `pdflongtermvalidation.cpp`: `CertificatePool` takes a reference per
  unique DER and frees in its destructor; `CMS_get1_certs` is pop-freed; `fetchIssuer` returns an
  owned object that is up-referenced by `pool.add` and then freed; `OCSP_basic_verify` is called
  with a store holding the issuer and `X509_V_FLAG_PARTIAL_CHAIN`, flags 0, so delegated responders
  need the OCSP-signing EKU and a chain to the issuer (`foreignOcspAnswerIsRejected` proves the
  same-name/other-key case); `OCSP_resp_find_status` binds the answer to the CertID of the
  certificate; `sk_X509_free` (not pop_free) on the issuers stack is right because no reference
  was taken. No double free or use after free found.
- Download bound: the `readyRead` handler aborts above the limit and the post-loop `readAll` is
  skipped; a hard deadline timer backs the transfer timeout; TLS errors are not ignored; only
  `http`/`https` with a host are accepted.
- DSS merge: old `/Certs`, `/OCSPs`, `/CRLs` references are kept, `/VRI` and other entries are
  copied, new streams are Flate streams written in the incremental update and (proven by
  `encryptedDocument`) encrypted by the document's security handler; dedup against the decoded
  existing streams (`existingStoreIsKept`).
- Signed bytes untouched: guarded by `startsWith` after the write, in both `addValidationData`
  and `extend`; `checkExtendedFile` requires the file to begin with them and the document
  timestamp (when present) to be valid over the whole file.
- `DocumentLockedNoChanges`: a document timestamp passes, the DSS update bypasses `sign()`;
  `certificationMustBeFirst` covers approval-after-P=1 refusal; `requireIncrementalUpdate`
  returns `IncrementalUpdateFailed` and both the plugin (lambda captures `stamp` by reference)
  and mobile (function rebuilt) drop the fingerprint and sign again.
- Pack flow: `signedLineCount` taken before `pop_back`, rotated last page keeps the image on
  the page and makes the digital signature invisible, the report uses a single `arg()` call.
- OpenSSL path of the CAdES signature: `CMS_sign` with `CMS_CADES | CMS_NO_SIGNING_TIME` on
  OpenSSL 3.5 (both macros present on this box); `cadesSignatureAttributes` proves
  signing-certificate-v2 present and signing-time absent; the signature verifies through the
  fork's PKCS#7 verifier; the signature timestamp is added as an unsigned attribute on the CMS
  object (`:507-539`).

## Is it PAdES per ETSI EN 319 142-1?

- **B-B**: yes on the OpenSSL path - `ETSI.CAdES.detached`, one SignerInfo, SHA-256,
  content-type + message-digest + signing-certificate-v2, no signing-time, `/M` present,
  signer certificate (and the PKCS#12 extras) in `certificates`. Windows path: plausibly yes,
  unproven (M5).
- **B-T**: yes - signature-time-stamp unsigned attribute from an RFC 3161 TSA (pre-existing
  code, exercised by the timestamp test's loopback TSA; `certReq` is set, so the TSU
  certificate is in the token).
- **B-LT**: structurally yes - DSS with the certificates of the signer's chain and the TSA's
  chain and an OCSP response or CRL for each non-root certificate, verified against the issuer.
  Not reliably: freshness relative to the signature time is not ensured and stale data is
  accepted (M1), `.p7c` issuers are missed (L2), and a self-signed signer (the program's default
  certificate) can never reach B-LT although the dialog says so (M4).
- **B-LTA**: structurally yes - a DocTimeStamp (`ETSI.RFC3161`, `/Type /DocTimeStamp`, no `/M`)
  written after the DSS, proven by `longTermDocumentTimestamp`. Not maintainable (L1).
- Nothing has been validated outside the program's own code (M5). The program's own viewer
  does not agree with its own output (M2).

The user guide's section 4 is accurate about what is attempted and that failures are reported;
it overstates in the places listed under L7 and, through the dialog text, in M4. The Legal note
is correct on s13(1)-(3) and s37-38; the Schedule 2 citation must be reconciled with the ledger.

## Test results (Debug tree `build-review`, `QT_QPA_PLATFORM=offscreen`, run one after another)

| Binary | Result | Output |
| --- | --- | --- |
| `UnitTestsLongTermValidation` | 66 passed, 0 failed, 0 skipped (33.7 s) | `out-r2a-ltv.txt` |
| `UnitTestsTimestamp` | 13 passed, 0 failed (2.7 s) | `out-r2a-timestamp.txt` |
| `UnitTestsSignatureBuilder` | 30 passed, 0 failed, 1 skipped (`publicTimestampAuthority`, needs `PDF4QT_TIMESTAMP_TEST_URL`) | `out-r2a-sigbuilder.txt` |
| `UnitTestsMobile` | 3 passed, 0 failed | `out-r2a-mobile.txt` |

Proof runs (temporary test `r2aStaleResponses` / `r2aStampSerialTruncation` added to
`tst_longtermvalidationtest.cpp`, then reverted; file byte-identical to the checkout, test binary
rebuilt from the clean source):
- stale OCSP (thisUpdate -30 d, no nextUpdate): `ok true ocspCount 1 crlCount 0 warnings QList()`
- expired CRL (nextUpdate -30 d, OCSP down): `ok true ocspCount 0 crlCount 1 warnings QList()`
- stamp with a 40-digit serial: `Issued by LAWtrust Advanced Electronic Signature C, serial 0123456789ABCDEF01234567`

No source change remains; no real OCSP, CRL or TSA service was contacted (all tests use loopback).
