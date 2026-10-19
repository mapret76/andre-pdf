# Andre PDF version history

Every version has a one-line summary (shown when an update is offered) and its
changes. Newest first. The release script refuses a version without an entry here.

## 1.0.1 - 2026-10-19

Summary: Signature Verification Report reads more signatures of other programs correctly, and small fixes from the independent review.

- Verification report: a signature whose certificate attribute names the whole chain is read correctly, also when the chain is not in the signature.
- Verification report: names of fields and timestamp authorities are shown without hidden control characters.
- Long-term validation: five minutes of clock difference allowed for revocation lists.

## 1.0.0 - 2026-10-19

Summary: First release of Andre PDF: certified PAdES contract signing with long-term validation and a verification report, signature library, initials, contract packs, size reduction, Word and Excel export, sharing, redline compare and font-faithful editing of scans.

- Sign and lock: certification signatures (no changes / form filling / comments), optional encryption before signing, and a check of the saved file.
- PAdES signatures (ETSI.CAdES.detached) with a trusted timestamp; long-term validation embeds the certificate chain, OCSP/CRL revocation data and a document timestamp (B-LTA).
- Signature Verification Report for third parties: certificate identity (subject, issuer, serial, fingerprints), chain, revocation, integrity, timestamp, PAdES level, lock and the status under the ECT Act; saved as PDF or HTML.
- Verification stamp with a QR code: signer, time, lock level, certificate fingerprint, issuer and serial, and the fingerprint of the file before signing.
- Signature library with four slots, imported from a photo or scan with the paper removed, encrypted for your Windows account.
- Initial every page, sign a contract from a checklist of its signature lines, and split and sign a contract pack in one batch.
- Reduce file size for e-mail, the smallest file or printing; a warning before rewriting a signed document.
- Export to Word (or open in Microsoft Word) and to Excel; share through the Windows share sheet.
- Redline report of two versions of a contract in Andre PDF Compare.
- Edit scanned text in its original font: font identification, free fonts found and installed, and a font built from the letters of the scan.
- Updates: a signed check for new versions with a summary of the changes, installed only when you agree; Help -> Version History.
- Credits: every component and its licence in Help -> About and THIRD-PARTY-NOTICES.txt.
