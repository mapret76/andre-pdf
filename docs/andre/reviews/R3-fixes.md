# R3 — what was done with the findings of the verification of the R2 fixes

Review: [R3.md](R3.md) on commit a20cb91 (fresh context). Most R2 fixes verified; partial:
M1 (signing-time comparison in the report only), M6 (bypass N1, redirects), L5 and L10 (single
signing flow only). No high findings. The fixes are in the commit that adds this file.

| Finding | Severity | Status | What changed |
| --- | --- | --- | --- |
| N1 "newest signature" chosen by byte ranges an earlier signer controls | medium | fixed | The caller names the signature just made (`addValidationData(..., signatureFieldName)`, `extend(...)`); only its chain and its timestamp are walked; an unknown name is an error. Test `onlyTheNamedSignatureIsWalked`. |
| N2 a signing-certificate-v2 naming the chain made the report INVALID | medium | fixed | The check gets the signer first and every other certificate of the signature. Test `verificationReportChainInSigningCertificate`. |
| N3 the 7-day rule refused current weekly/monthly CRLs | medium | fixed | A CRL counts when issued in the past and its next update is still to come (RFC 5280); an OCSP answer with a next update counts until then, without one for 7 days. |
| N4 budget overrun, >32 certificates skipped silently | low | fixed | Every request gets what is left of the one-minute budget; skipped certificates are reported. |
| N5 redirects to any host | low | fixed | Same-origin redirects only (at most 3). |
| N6 TSA fallback missing in the pack flow | low | documented | The user guide says the pack flow reports the failure. |
| N7 signature dictionary texts not cleaned | low | fixed | Name, reason, location, contact cleaned like the certificate names. |
| N8 identity and trust rows from different certificates | low | open | The trust row comes from the upstream verifier (first issuer/serial match); with a decoy it can only be worse, never better, than the identity row. |
| N9 first TSA request without the system proxy | low | fixed | The timestamp request uses the system proxy too. |
| N10 .p7c takes the first name match | low | fixed | A candidate must verify the certificate. |
| guide: "the report says what those bytes are" | low | fixed | Reworded. |
| B1 ticked before its gate | — | fixed | B1 back to "[~]" until Andre's Windows-store / Acrobat check. |
