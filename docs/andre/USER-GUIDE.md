# Andre PDF - user guide

Andre PDF is a free PDF editor for Windows (Android to follow), built on PDF4QT by
Jakub Melka. This guide covers the features added for contracts, signing and scans.
Everything else (reading, annotations, forms, OCR, pages, encryption, printing) is
PDF4QT's own editor and works as its menus describe.

## Install

1. Download the `.msi` from the latest successful **Windows_MSI** run on
   github.com/mapret76/andre-pdf → Actions (artifact "…msi").
2. Run it. Windows SmartScreen warns, because the installer is not code-signed:
   **More info → Run anyway**.
3. Start **Andre PDF Editor** from the Start menu. Andre PDF installs alongside any
   upstream PDF4QT and does not replace it.

Help → About lists every component the program is built from, with its licence;
`THIRD-PARTY-NOTICES.txt` in the program folder holds the full licence texts.

## The order of work on a contract

**Split → reduce size → initial → sign every line → lock.** Anything that rewrites
the file after the first signature breaks it, and the program warns before it does.

## 1. Your signatures (once)

Signature menu → **Signature Library…**
- Sign a white sheet of paper four times (full signature, initials, …), photograph or
  scan it, and **Import from Image…** into a slot. The paper is removed automatically.
  Move the **Ink threshold** slider if faint strokes go missing, and choose the ink
  colour if you like.
- Give each slot a name ("Initials").
- The library is kept in your Windows profile, encrypted for your Windows account.

## 2. Reduce the file size (optional, before signing)

Tools → **Reduce File Size…** → e-mail (150 dpi), smallest (110 dpi) or printing
(300 dpi). The dialog shows the size before and after; check the pages, then save.

## 3. Initial every page

Signature menu → **Initial All Pages…** → choose the initials slot, the corner, the
size; by default the last page (with the full signature) is left out.

## 4. Sign a contract

Signature menu → **Sign Contract…**
- The program finds the signature lines: signature fields, and lines labelled
  "Signature", "Signed", "Witness", "Initials", "Handtekening", "Getuie", …
- Decide **every** line: **I sign** or **Other party**. "Sign & Lock" stays disabled
  until every line is decided.
- Your signature image goes on each of your lines; the last one becomes the visible
  digital signature with the verification stamp.
- In the Sign dialog choose your certificate and enter its password once. **Lock
  (certify)** is preset: "No changes allowed" if you sign every line, "Allow form
  filling and signing" if the other party still has to sign.
- Optional: **Encrypt** with a password to open the file. Encryption is applied
  before the signature, so it never breaks it.
- After saving, the program reads the file back and shows what a reader will see.

The **verification stamp** under the signature shows who signed, when, how the
document is locked and fingerprints of your certificate and of the file before
signing; the QR code holds the same, readable by any phone. The proof is the digital
signature itself, which Adobe Reader and other readers check in their signature panel.

To sign by hand somewhere else, use **Place <signature>** in the Signature Toolbox,
then **Sign Digitally With Certificate**.

## 5. A pack of contracts in one file

Signature menu → **Split & Sign Contract Pack…**
- The contracts are taken from the bookmarks, or **From Page Numbers** (where the
  numbering restarts at "Page 1 of …"), or typed as page ranges ("1-5, 6-9").
- Choose the initials, the signature, and whether you sign every line or only lines
  labelled with your name or company. Choose the folder.
- One certificate password signs them all. Each contract is saved on its own, signed,
  locked and checked; a report lists the results. Existing files are never overwritten.

## 6. Share

File → **Share…** opens the Windows share sheet (Outlook, WhatsApp, Teams, nearby
sharing, whatever is installed). File → **Send by E-Mail** attaches the file to a new
mail in your mail program. Both share the *saved* file.

## 7. Word and Excel

File → **Export to Word…**: with Microsoft Word installed you can **Open in Word**
(Word converts the PDF and keeps its layout) or export the text as a `.docx`.
File → **Export to Excel…**: every page becomes a sheet; tables are split into
cells and amounts become numbers. For a single table, the **Select Table** tool copies
it for pasting into Excel.

## 8. Compare two versions (redline)

**Andre PDF Compare** → open the old version left and the new version right →
**Create Redline Report…**: a PDF listing every changed passage with its page,
deleted text struck through in red and inserted text underlined in blue.

## 9. Edit a scanned page in its original font

1. Run OCR on the scan (Tools → OCR) so the text is known.
2. Scan Text menu → **Edit Scanned Text…** and drag a rectangle over the words.
3. The dialog shows the scan, the text, and the fonts most like it.
   - The font of the PDF itself is listed first when it is missing on this computer.
   - **Search Free Fonts Online…** tries common free document fonts (Google Fonts and
     others, from Fontsource). Only the fonts are downloaded; the scan stays on the PC.
   - **Install Font…** downloads a free font and installs it for your user (no admin
     rights). Commercial fonts are never downloaded; their free look-alikes with the
     same letter widths are named instead (Arial → Liberation Sans, Calibri → Carlito, …).
   - **Build Font from This Scan** makes a font from the letters of the page itself,
     when nothing else is close.
4. Type the new text, check the preview, **Replace Text**. The new text is written in
   the chosen font, size and ink colour, on the colour of the paper, and the font is
   embedded in the PDF. Run OCR on that page again afterwards: the old hidden OCR text
   of the page is removed so it cannot reveal the old words.

## Android

The Android app (open, read, signature library, sign every line or tap where to sign,
lock, certificate from a `.pfx` file) is written and tested on the desktop. The APK
needs a one-time GitHub permission first. See "Waiting on Andre" in `ANDRE-PDF.md`.

## Legal note

The stamp says "Electronically signed". Under the ECT Act an *advanced* electronic
signature needs a certificate from an SAAA-accredited provider (for example LAWtrust
AeSign); a self-made certificate gives an ordinary electronic signature.
