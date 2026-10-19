#!/usr/bin/env bash
# Andre PDF release, on andre-vault: tools/andre-release.sh <run id of a successful Windows_MSI run>
#
# The run must have built the commit whose CMakeLists.txt sets ANDRE_PDF_VERSION to the
# version released, and CHANGELOG.md of that commit must have its entry. The script signs
# the update manifest with the release key (~/.config/andre-pdf/release-signing-key.pem,
# owner-only, Andre's choice 2026-10-19), verifies the signature with the public key of the
# program, and publishes the GitHub release with the installer, the manifest and its signature.
set -euo pipefail

repo=mapret76/andre-pdf
key="$HOME/.config/andre-pdf/release-signing-key.pem"
die() { echo "andre-release: $*" >&2; exit 1; }

[ $# -eq 1 ] || die "usage: $0 <Windows_MSI run id>"
run=$1
cd "$(git rev-parse --show-toplevel)"
[ -r "$key" ] || die "the release key $key is missing"

sha=$(gh run view "$run" -R "$repo" --json headSha,conclusion,workflowName -q 'select(.conclusion=="success" and .workflowName=="Windows_MSI") | .headSha')
[ -n "$sha" ] || die "run $run is not a successful Windows_MSI run"
version=$(git show "$sha:CMakeLists.txt" | sed -n 's/^set(ANDRE_PDF_VERSION \([0-9.]*\))\r\{0,1\}$/\1/p')
[ -n "$version" ] || die "commit $sha sets no ANDRE_PDF_VERSION"
if gh release view "v$version" -R "$repo" >/dev/null 2>&1; then die "release v$version exists already"; fi

work=$(mktemp -d "$HOME/projects/andre-pdf-release.XXXXXX")   # not /tmp, it is RAM on the vault
trap 'rm -rf "$work"' EXIT
git show "$sha:CHANGELOG.md" > "$work/CHANGELOG.md"

gh run download "$run" -R "$repo" -p '*.msi' -D "$work/artifact"
msi=$(find "$work/artifact" -name '*.msi' | head -1)
[ -f "$msi" ] || die "the run has no MSI"
case "$(basename "$msi")" in *"_$version.msi") ;; *) die "the MSI $(basename "$msi") is not version $version" ;; esac
cp "$msi" "$work/AndrePDF_$version.msi"

url="https://github.com/$repo/releases/download/v$version/AndrePDF_$version.msi"
python3 tools/make_update_manifest.py --version "$version" --msi "$work/AndrePDF_$version.msi" --url "$url" --changelog "$work/CHANGELOG.md" > "$work/andre-pdf-update.json"
openssl dgst -sha256 -sign "$key" "$work/andre-pdf-update.json" > "$work/signature.der"
base64 -w0 "$work/signature.der" > "$work/andre-pdf-update.json.sig"
git show "$sha:Pdf4QtLibCore/andre-pdf-release-key.pub.pem" > "$work/release-key.pub.pem"
openssl dgst -sha256 -verify "$work/release-key.pub.pem" -signature "$work/signature.der" "$work/andre-pdf-update.json" >/dev/null \
    || die "the signature does not verify with the public key of the program"

python3 - "$version" "$work/CHANGELOG.md" > "$work/notes.md" <<'PY'
import sys, re
version, path = sys.argv[1], sys.argv[2]
text = open(path, encoding='utf-8').read()
m = re.search(r'^## ' + re.escape(version) + r'\b.*?$(.*?)(?=^## |\Z)', text, re.S | re.M)
print(m.group(1).strip() if m else '')
print('\n---\nInstall: run the .msi (not code-signed: SmartScreen -> More info -> Run anyway). Andre PDF updates itself from this release (signed manifest).')
PY

gh release create "v$version" -R "$repo" --target "$sha" --title "Andre PDF $version" --notes-file "$work/notes.md" \
    "$work/AndrePDF_$version.msi" "$work/andre-pdf-update.json" "$work/andre-pdf-update.json.sig"
echo "andre-release: v$version published from $sha (run $run), SHA-256 $(sha256sum "$work/AndrePDF_$version.msi" | cut -d' ' -f1)"
