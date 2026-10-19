# R4 — what was done with the findings of the verification of the R3 fixes

Review: [R4.md](R4.md) on commit 03a88ce (fresh context). Verified: N1, N4, N5, N9, N10, the guide
wording and B1; N3 with loose ends; N2 and N7 partly. The field name cannot be spoofed in practice;
passing the chain to the signing-certificate check does not re-open the decoy case (proven).
No high findings. Fixes in version 1.0.1.

| Finding | Severity | Status | What changed |
| --- | --- | --- | --- |
| M1 a signing-certificate-v2 naming a chain whose root is not in the signature reads INVALID | medium | fixed | The attribute is reduced to its first certificate ID (the signer, RFC 5035) before the check; the chain IDs do not matter any more. Test `verificationReportChainInSigningCertificate` with and without the root. |
| L1 field and timestamp-authority names not cleaned | low | fixed | |
| L2 duplicate field names / last match wins | low | open | Names end in the time in milliseconds. |
| L3 no clock allowance for CRLs | low | fixed | Five minutes, as for OCSP. CRL warning when OCSP answered: left out (the status is known). |
| L4 flaky `existingStoreIsKept` | low | fixed | The test authority publishes one CRL per state. |
| Windows link error (Extension not exported) | — | fixed | Found by the Windows CI after 03a88ce; `PDFLongTermValidation::Extension` exported. |
