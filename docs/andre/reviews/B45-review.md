# B4/B5 review: version history and the self-updater

Reviewed commits `54c0e94` and `0ac9e0a` (worktree `/home/andre/projects/andre-pdf-review`, Debug core build `build-review`).
Scope: `pdfversionhistory.{h,cpp}`, `CHANGELOG.md`, `andrepdf.qrc`, `pdfupdatechecker.{h,cpp}`,
`Pdf4QtLibGui/pdfupdatedialog.*`, `pdfversionhistorydialog.*`, `pdfprogramcontroller.cpp` (`checkForUpdates`, `offerUpdate`),
`pdfeditormainwindow.cpp` (startup), `tools/make_update_manifest.py`, `tools/andre-release.sh`, `UnitTests/tst_updatetest.cpp`.
The GUI files were reviewed by reading only, because Pdf4QtLibGui is not built in this tree.

Method: I read all the code in scope, then ran the existing suite. I added nine temporary probes (P1-P9) to
`tst_updatetest.cpp`, built them with `ninja UnitTestsUpdate`, ran them, and removed them again. I also dry-ran the
release script with a stub `gh` (no network), a throwaway P-256 key and a scratch `HOME`, so the real key under
`~/.config` was never touched. Each finding says how it was established: **proven** means a probe or dry run
showed the behaviour; **reading** means by code reading only.

## Summary

The cryptographic core is sound. The signature covers the exact bytes that are parsed, so no field escapes it.
These checks are all in place:

- the key is embedded and the check fails closed;
- an old or replayed manifest (version less than or equal to the installed one) is ignored, and the MSI's
  `MajorUpgrade` also blocks a downgrade;
- the installer URL must be HTTPS on a GitHub host;
- size and SHA-256 are checked, with limits on every download;
- msiexec is started from System32 with an argument list and a file name we choose (no injection);
- nothing is downloaded or started without two explicit clicks.

The problems are around that core:

- the release script cannot publish at all, as written (High);
- ordinary changelog text can blank the "what changes" summary;
- the window between the hash check and msiexec runs at user pace, for an elevated per-machine install;
- a running download cannot be cancelled;
- the release script signs whatever a successful `Windows_MSI` run produced, from any branch.

## Findings

### High

**H1. `andre-release.sh` always stops with "the run has no MSI": no release, and so no update, can be published** (proven)
`tools/andre-release.sh:31` `msi=$(find "$work/artifact" -name '*.msi' | head -1)`.
- `gh run download -p` puts each artifact in its own directory named after the artifact (gh 2.97 help: "extracted
  under separate directories based on the artifact name"; only a single `-n` name skips the directory).
- The artifact itself is named `JakubMelka.PDF4QT_<version>.msi` (`.github/workflows/WindowsInstall.yml:182,234`).
- So the first hit of `find` is the *directory* `artifact/JakubMelka.PDF4QT_1.0.0.msi`, and `[ -f "$msi" ]` fails.

The dry run with a stub `gh` that reproduces this layout died with exactly that message. With `-type f` added
(scratch copy only), the same dry run went on to sign, verify and reach `gh release create` with the three assets.
The bug fails closed, but B5 cannot ship until it is fixed.
Fix: `find "$work/artifact" -type f -name "*_$version.msi"`, require exactly one match, and add a stubbed-`gh` test of the script.

### Medium

**M1. Changelog or manifest text is rendered as Markdown with raw HTML: ordinary text can blank the summary** (proven)
`Pdf4QtLibCore/sources/pdfversionhistory.cpp:122-133` (`toMarkdown` pastes summary and changes unescaped),
`Pdf4QtLibGui/pdfupdatedialog.cpp:48` and `pdfversionhistorydialog.cpp:48` (`QTextBrowser::setMarkdown`, GitHub
dialect, HTML enabled).
- Probe P9: a release with the summary `Press <Enter> to sign; files <5 MB are not compressed.` and the changes
  `Shortcut <Ctrl+S> saves.`, `A list <!-- hidden`, `Last change.` renders as plain text `"1.2.0\nPress \n\n\n"`.
  `<Enter>` is a valid HTML start tag, and it swallows the rest of the summary and **every change**.
- Probe P5: `<a href=...>` survives as a live link, and `<font>` drops text.
- `PDFUpdateDialog` keeps the default `openLinks=true`, so clicking a link navigates the browser inside the
  dialog and the summary disappears. `PDFVersionHistoryDialog` opens any link externally (line 41).
- A newline inside a JSON `summary` breaks the `**...**` wrapper and can fake a `## 99.0.0` heading (P6).

The source is trusted (signed manifest, or the repo's CHANGELOG), so this is not code execution. But it defeats the
owner requirement "always shows a summary", through text that is easy to write by accident.
Fix:
- use `m_browser->document()->setMarkdown(md, QTextDocument::MarkdownDialectGitHub | QTextDocument::MarkdownNoHTML)`
  (P5b: tags then show as literal text);
- escape the Markdown specials (`*_[]<>#\` and newlines) in `toMarkdown`;
- call `setOpenLinks(false)` in the update dialog;
- optionally reject `<` and newlines in `make_update_manifest.py`.

**M2. Time-of-check/time-of-use (TOCTOU) gap between the SHA-256 check and msiexec, for an elevated install** (reading)
`pdfupdatechecker.cpp:304-329` writes the installer to a fixed name (`CacheLocation/updates/AndrePDF_<v>.msi`), closes
it, re-reads it to hash it, and returns the path. `pdfprogramcontroller.cpp:1785-1791` then waits for the user's
"Close Andre PDF and install it now?" (any length of time) and starts `msiexec /i <path>`.
- The package is `InstallScope="perMachine"` (`WixInstaller/Product.wxs.in:8`), so msiexec elevates.
- The MSI is not code-signed, so the UAC prompt shows "unknown publisher" whatever the file is.
- So a non-admin process running as the user can replace the verified file during that window and ride the UAC
  prompt the user expects. This is the classic "updater gives local privilege escalation with consent" pattern.

The same fixed name with `Truncate` also lets two instances, or a repeated "Check for Updates", overwrite a file
that was already verified: msiexec then gets a partial or missing file.
Fix:
- download to a unique `QSaveFile`/`QTemporaryFile` in the directory, verify, then rename;
- re-hash right before `startInstaller` and keep the file open without share-write/delete
  (`CreateFile(..., FILE_SHARE_READ)`) until msiexec has started;
- long term, Authenticode-sign the MSI and check the signature before starting it.

**M3. The download cannot be cancelled, and the timeout only fires when the connection goes idle** (reading)
`pdfprogramcontroller.cpp:1755` creates the `QProgressDialog` with no cancel button, window-modal, then runs a nested
loop followed by `thread->wait()`. `pdfupdatechecker.cpp:76` `setTransferTimeout(60000)` aborts only after 60 s
*without data*.
- With an installer of up to 600 MiB allowed and a slow connection, the user is locked out of the editor for the
  whole download.
- A trickling connection can keep it alive without end.
- An OS log-off during the download blocks in `thread->wait()`.

Fix: add a Cancel button that sets an atomic flag, checked in `readyRead` to call `reply->abort()`. Add an overall
deadline that depends on the size.

**M4. The release script does not bind the signed release to reviewed code** (reading)
`tools/andre-release.sh:20` only checks `conclusion=="success"` and `workflowName=="Windows_MSI"`.
- `Windows_MSI` is `workflow_dispatch`, so anyone with write access can run it on any branch or commit.
- The script then signs the MSI that run produced, and the signature is the *only* trust anchor, because the MSI is
  unsigned.
- A wrong run id, or a collaborator's branch run, becomes a validly signed update for every installation.

Further gaps:
- `git show "$sha:..."` needs the commit in the local clone, and the script never runs `git fetch`.
- The script never checks that the new version is newer than the current `latest`. A lower version published by
  mistake becomes `latest` on GitHub and silently freezes the updates of every client.

Fix:
- require `.headBranch=="master"` and `.event=="workflow_dispatch"`;
- run `git fetch origin && git merge-base --is-ancestor "$sha" origin/master`;
- compare the new version with `gh release view --json tagName` of the latest release;
- print the sha, version and summary and ask for `yes` before signing.

### Low

- **L1. Redirects are not held to the host allow-list** (reading). `pdfupdatechecker.cpp:77` `NoLessSafeRedirectPolicy`
  follows any HTTPS redirect. The allow-list (line 263) only checks the manifest URL, so the comment at line 69-70
  over-claims. The SHA-256 is pinned, so the impact is limited to privacy and bandwidth.
  Fix: `UserVerifiedRedirectPolicy` plus a `redirected` handler that checks `allowedHosts`.
- **L2. No freshness in the manifest; two separate "latest" requests** (reading). An old signed manifest, or `latest`
  pointing at an older release, freezes updates with no error. The manifest and the `.sig` come from two
  `releases/latest/download` requests, so if `latest` changes between them (during a release), the result is a
  frightening "not signed with the release key" message.
  Fix: an `issued` field (warn when it is older than N months); resolve the tag once and fetch both files from it.
- **L3. Loose validation of signed fields** (proven, P3/P4).
  - `isValidVersion` (`pdfversionhistory.cpp:37`) uses `$`, so `"1.0.0\n"` is accepted.
  - `size` is read as a double and truncated (2048.9 is accepted as 2048).
  - `QByteArray::fromHex` skips non-hex characters (`"zz"+hex` is accepted).
  - The URL may carry userinfo and a port (`https://x@github.com:4444/...` is accepted).

  The fields are signed, so this is hardening only. Fix: `QRegularExpression::anchoredPattern`, an integer `size`,
  exactly 64 hex characters, and no userinfo or port.
- **L4. Two Enter presses start the installer from an automatic popup** (reading). "Download and Install" is the
  default button (`pdfupdatedialog.cpp:55`), and so is Yes in the confirmation (`pdfprogramcontroller.cpp:1786`). The
  startup dialog can appear while the user is typing. The MSI wizard and UAC still follow, so the install is not
  silent, but the app closes. Fix: make "Remind Me Later" the default for automatic offers.
- **L5. Quit after starting the installer** (reading). `QApplication::quit()` (`pdfprogramcontroller.cpp:1796`) goes
  through `closeEvent`. That asks the save question a second time (a user who answered "No" is asked again), and
  Cancel, or `canClose()==false` while a document is loading, leaves the app running while msiexec runs ("files in
  use"). Fix: close the window first and start the installer from `aboutToQuit`, or mark the document as discarded.
- **L6. Once-a-day logic** (reading). `pdfprogramcontroller.cpp:1683-1689` writes `LastUpdateCheck` *before* the check
  and compares with `>=`. If the computer is offline at start, there is no check until tomorrow. A clock that was
  once set into the future disables automatic checks until that date. Fix: compare with `!=`, and store the date
  after a completed check.
- **L7/L8. Check threads** (reading). `QThread::create` (line 1700) has no owner. If the app quits while a check hangs
  (up to 2 x 60 s), the thread is never joined, which can crash at shutdown. A manual check while another check is
  running starts a second thread, a second dialog and, with M2, a clobbered download. Fix: one in-flight flag, and
  join the thread on `aboutToQuit`.
- **L9. Release script details** (proven/reading).
  - The notes regex (`andre-release.sh:48`, `\b`) takes the `## 1.0.0` section when releasing `1.0` (proven in Python).
  - `tools/make_update_manifest.py` runs from the working tree, not from `$sha` (line 37).
  - The mode of the key file is not checked (`stat -c %a` should be 600).
  - The signature is verified only against the public key of `$sha`. After a key rotation, the clients already
    installed would reject the release: verify against the previous release's key too.
  - `gh release create` (line 53) uploads to a draft and then publishes. A failure can leave a draft, which makes the
    next run die with "exists already": document `gh release delete v$version --cleanup-tag`.
- **L10. The downloaded MSIs are never removed** from `CacheLocation/updates` (about 100 MB per version).
- **L11. The C++ and Python changelog parsers disagree.** `PDFVersionHistory::parse` drops a heading with an invalid
  version (for example 5 components) and attaches its lines to the previous release; the Python parser keeps it.
  CHANGELOG mistakes therefore go unnoticed. Fix: make both reject such a heading.
- **L12. Other platforms.** On Linux the startup check runs every day, and a manual check says "Version X has no
  installer for this computer". The Android app has neither a version history nor an update path.
- **L13. Test gaps.**
  - No test covers `PDFUpdateDialog`, `PDFVersionHistoryDialog`, `showWhatsNewOnce`, `offerUpdate` or the skip and
    later logic.
  - `checkAndDownload` runs only in test mode (HTTP, `SameOriginRedirectPolicy`), so the production redirect policy
    is never exercised.
  - `tst_updatetest.cpp:274` ignores the result of `QFile::open` (a `-Wunused-result` warning; the project requires
    warning-free code).
- **L14. Notes on the trust anchor.** `verifySignature` accepts any 256-bit EC key, not P-256 only
  (`pdfupdatechecker.cpp:177`). `setTestConfiguration` is exported in the production library and can replace the
  trust anchor; only the tests call it. Fix: check `EVP_PKEY_get_group_name == "prime256v1"`, and compile the test
  hook only into test builds.

### Checked and found sound

- The signature is over the raw manifest bytes, and `parseManifest` sees the same bytes (no canonicalisation, no
  unsigned field).
- Duplicate JSON keys: Qt and Python both keep the last value (P1), and the manifest tool emits `json.dumps` of a
  dict (no duplicates), so there is no ambiguity.
- Downgrade or replay: a version less than or equal to the installed one returns nothing, with no error.
  Intermediate versions are listed between the installed and the new version.
- The installer URL must be HTTPS on an allowed host; the size must be between 1 byte and 600 MiB and the SHA-256
  exactly 32 bytes; a mismatching or partial download is deleted.
- Limits: manifest 1 MiB, signature 4 KiB, installer equal to the manifest size, each enforced per chunk.
- msiexec is started as `%SystemRoot%\System32\msiexec.exe` with an argument list (no shell). The file name is built
  from a version restricted to digits and dots, so there is no command-line injection.
- Consent: nothing is downloaded without "Download and Install", and nothing is started without "Yes". Esc or close
  means "Later". "Skip" suppresses only the automatic offer of exactly that version; a manual check still offers it.
- The check, the download and the hashing run off the GUI thread. The queued progress calls are bound to the dialog
  object, so they are safe after it is destroyed.
- The release script passes `bash -n` and every expansion is quoted. It takes the CMakeLists, CHANGELOG and public
  key of the run's commit, and verifies the signature before publishing. Its temporary directory under `~/projects`
  is removed by the trap. With the wrong key it fails closed before `gh release create` (proven).

## Requirements met?

| Owner requirement | Met? | Notes |
| --- | --- | --- |
| Checks for new versions | Yes | Daily at start (can be turned off) and Help -> Check for Updates. L6 delays checks after an offline start. |
| Downloads and installs only when the user agrees | Yes | Two explicit confirmations, then the msiexec UI. M2 is a gap in integrity, not consent; L4 is a default-button risk. |
| Always shows a summary of what changes in the next version | Partly | It is enforced by `parseManifest` and `make_update_manifest.py`, but M1 lets ordinary text such as `<Enter>` blank it. |
| Users can see the full version history | Yes on desktop | Help -> Version History, and "What's New" once after an update. Same rendering issue (M1). None on Android. |

H1 means the release side cannot publish yet, so the updater cannot deliver an update until it is fixed.

## Test results

- `UnitTestsUpdate` (rebuilt after the probes were removed): 7 passed, 0 failed (`signatureVerification`,
  `manifestRules`, `checkAndDownload`, `releaseKeyIsEmbedded`, `releaseToolsInterop`, plus init and cleanup). The
  output is in `out-b45-update.txt`. One compiler warning: `tst_updatetest.cpp:274`, unused result of `QFile::open`.
- `SignatureBuilderTest::versionHistory`: PASS (from `out-UnitTestsSignatureBuilder.txt` of the parallel review).
- Temporary probes P1-P9 (removed again):
  - P1: on a duplicate `version` key, the last one wins.
  - P2: manifest `1.1` with release `1.1.0` is refused.
  - P3: `"1.0.0\n"` is a valid version.
  - P4: fractional size, non-hex characters and a URL with port and userinfo are accepted.
  - P5/P9: HTML is interpreted, and `<Enter>` blanks the summary and the changes.
  - P5b: `MarkdownNoHTML` fixes it.
  - P6: a newline in the summary injects a heading.
  - P7: size `1e300` is refused.
  - P8: junk entries in `releases` are ignored.
- Release script dry run (stub `gh`, throwaway key, scratch `HOME`, no network):
  - as committed, it dies with "the run has no MSI" (H1);
  - with `-type f`, it fails closed against the embedded public key;
  - with the throwaway public key as well, it reaches `gh release create` with the MSI, the manifest and the `.sig`,
    and the expected notes.
- All temporary source changes were reverted (`git status` shows only report files and untracked `out-*.txt`).
  `scratch-b45/` was deleted.
