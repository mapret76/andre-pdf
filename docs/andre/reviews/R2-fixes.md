# R2 — what was done with the findings of the review of B1–B3 and of the R1 fixes

Reviews: [R2-A.md](R2-A.md) (B1 PAdES and long-term validation, B3 stamp, signer changes) and
[R2-B.md](R2-B.md) (B2 verification report, and the R1 fixes: all verified, L7/L8 and L13
partly), both on commit 1f98cfd by reviewers with a fresh context. No high findings. The fixes
are in the commit that adds this file.

## R2-A

| Finding | Severity | Status | What changed |
| --- | --- | --- | --- |
| M1 stale revocation data embedded silently | medium | fixed | OCSP answers older than 7 days are refused (also without nextUpdate); a CRL must have a next update still to come and a last update at most 7 days old. Test `staleRevocationDataIsRefused`. Comparison with the time of the signature: the report says when an answer predates the signature (R2-B L2). |
| M2 the program's own verifier ignores the stored OCSP answers | medium | fixed (CRL) / accepted (coverage) | The CRL is stored as well whenever the certificate names one, so the CRL-only verifier finds the status. The "bytes not covered" warning after validation data is upstream behaviour; the user guide explains it and the report names those bytes. |
| M3 stamp cuts the serial and the issuer | medium | fixed | The serial is printed in full, in groups of four over as many lines as needed; a long issuer name ends in "…". Test with a 40-digit serial. |
| M4 "PAdES B-LTA" claimed when not true | medium | fixed | The saved-file dialog takes the level from the verification report of the saved file ("format: PAdES baseline B-…"); the extension report no longer names a level. |
| M5 Windows store / smart-card path never exercised, no external validator | medium | open (gate) | Needs Windows with a certificate in the store and Acrobat / the EU DSS validator: added to "Waiting on Andre" (sign with a Windows-store certificate, open the B-LTA file with a "No changes" lock in Acrobat Reader). CI compiles the code. |
| M6 any certificate in the document drives network requests | medium | fixed | Only the newest signature (the one just made) and its timestamp are walked; responder certificates are stored, not walked; at most 32 certificates; one minute for all requests together; at most 3 redirects. |
| L1 no re-extension of an existing file | low | open | |
| L2 PKCS#7 "CA issuers" answers | low | fixed | `.p7c` answers are read. |
| L3 stored certificates not used for the chain | low | fixed | Certificates already in the DSS complete the chain. |
| L4 delta / partitioned CRLs | low | open | |
| L5 signing fails when the TSA is unreachable | low | fixed | The program offers to sign without a timestamp (and then adds no long-term data). |
| L6 checkExtendedFile without a timestamp | low | accepted | The saved file must begin with the checked signed bytes and open. |
| L7 user guide and ledger wording | low | fixed | PAdES described as the European standard; the accredited-certificate path described as the provider's process via the Windows store; Android signs B-B only; the issuer line proves nothing by itself; s4(4) in the ledger. |
| L8 older OpenSSL silently without the CAdES flags | low | accepted | vcpkg and the Linux CI use OpenSSL 3.5. |
| L9 "No changes" refusal trusts /Perms | low | accepted | Refusing is the safe side. |
| L10 no system proxy | low | fixed | System proxy settings for OCSP / CRL / CA issuers. |
| L11 32 MB CRL | low | fixed | At most 10 MB. |
| L12 Unicode line separators | low | fixed | Removed from names in the stamp and in the report. |

## R2-B

| Finding | Severity | Status | What changed |
| --- | --- | --- | --- |
| B2-M1 "covers the whole file" with a hole in the signed span | medium | fixed | The two byte ranges must leave out exactly the hexadecimal signature string; anything else makes the integrity INVALID. Test `verificationReportGapInSignedRange`. |
| B2-M2 a decoy certificate reported as the signer | medium | fixed | The signer's certificate must match the signed signing-certificate(-v2) attribute (`OSSL_ESS_check_signing_certs`) and verify the signed attributes; without such a certificate the integrity is INVALID and no identity is shown. Test `verificationReportIgnoresDecoyCertificate`. |
| B2-M3 ECT Act paragraph keyed on the issuer's name | medium | fixed | Three cases: self-signed; chain not verified ("the issuer is only a claim"); trusted chain ("may be an advanced electronic signature if the whole signing process is the provider's accredited one: ask the provider"). "Published list" instead of "register". |
| B2-L1 PAdES level overstated | low | fixed | B-T only with a verified timestamp; B-LT only with revocation data for every certificate of the chain below the root (the timestamp authority's chain is not required yet). |
| B2-L2 revocation time not related to signing | low | fixed | The report says when the answer was produced before the signature. |
| B2-L3 no try/catch around the report | low | fixed | |
| B2-L4 format characters in names | low | fixed | Removed in the report. |
| B2-L5 self-signed decided by names | low | fixed | The certificate's own signature is verified. |
| B2-L6 smaller points | low | partly | Prompt wording and "Save as PDF" failure fixed; duplicate field names and the GUI-thread run left. |
| R1 L7/L8 thread not joined at quit | low | open | The thread object is never destroyed while running; a hang at quit is bounded by the request deadlines. |
| R1 L13 no dialog tests | low | open | |
