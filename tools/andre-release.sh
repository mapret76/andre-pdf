#!/usr/bin/env bash
# Andre PDF release, on andre-vault: tools/andre-release.sh <run id of a successful Windows_MSI run>
#
# The run must have built a commit of master whose CMakeLists.txt sets ANDRE_PDF_VERSION to
# the version released (newer than the latest release), and CHANGELOG.md of that commit must
# have its entry. The script signs the update manifest with the release key
# (~/.config/andre-pdf/release-signing-key.pem, owner-only, Andre's choice 2026-10-19),
# verifies the signature with the public key of the program, shows what it is about to
# publish, and after "yes" publishes the GitHub release with the installer, the manifest and
# its signature.
#
# A run that fails in "gh release create" can leave a draft release:
#   gh release delete v<version> -R mapret76/andre-pdf --cleanup-tag
set -euo pipefail

repo=mapret76/andre-pdf
key="$HOME/.config/andre-pdf/release-signing-key.pem"
die() { echo "andre-release: $*" >&2; exit 1; }

[ $# -eq 1 ] || die "usage: $0 <Windows_MSI run id>"
run=$1
cd "$(git rev-parse --show-toplevel)"
[ -r "$key" ] || die "the release key $key is missing"
[ "$(stat -c %a "$key")" = 600 ] || die "the release key $key must be readable by its owner only (chmod 600)"

# Only a build of master is signed: the signature is the only trust anchor of the update
sha=$(gh run view "$run" -R "$repo" --json headSha,headBranch,conclusion,workflowName \
    -q 'select(.conclusion=="success" and .workflowName=="Windows_MSI" and .headBranch=="master") | .headSha')
[ -n "$sha" ] || die "run $run is not a successful Windows_MSI run of master"
git fetch -q origin master
git merge-base --is-ancestor "$sha" origin/master || die "commit $sha is not on origin/master"

version=$(git show "$sha:CMakeLists.txt" | sed -n 's/^set(ANDRE_PDF_VERSION \([0-9.]*\))\r\{0,1\}$/\1/p')
[ -n "$version" ] || die "commit $sha sets no ANDRE_PDF_VERSION"
if gh release view "v$version" -R "$repo" >/dev/null 2>&1; then
    die "release v$version exists already (a failed run leaves a draft: gh release delete v$version -R $repo --cleanup-tag)"
fi

# A lower version published as "latest" would stop the updates of every installation
latest=$(gh release view -R "$repo" --json tagName -q .tagName 2>/dev/null || true)
if [ -n "$latest" ]; then
    newest=$(printf '%s\n%s\n' "${latest#v}" "$version" | sort -V | tail -1)
    { [ "$newest" = "$version" ] && [ "${latest#v}" != "$version" ]; } || die "version $version is not newer than the latest release $latest"
fi

work=$(mktemp -d "$HOME/projects/andre-pdf-release.XXXXXX")   # not /tmp, it is RAM on the vault
trap 'rm -rf "$work"' EXIT
git show "$sha:CHANGELOG.md" > "$work/CHANGELOG.md"
git show "$sha:tools/make_update_manifest.py" > "$work/make_update_manifest.py"
git show "$sha:Pdf4QtLibCore/andre-pdf-release-key.pub.pem" > "$work/release-key.pub.pem"

# gh puts each artifact into a directory of its name, which ends in .msi itself
gh run download "$run" -R "$repo" -p '*.msi' -D "$work/artifact"
mapfile -t msis < <(find "$work/artifact" -type f -name "*_$version.msi")
[ "${#msis[@]}" -eq 1 ] || die "the run has ${#msis[@]} MSI files of version $version, expected one"
cp "${msis[0]}" "$work/AndrePDF_$version.msi"

url="https://github.com/$repo/releases/download/v$version/AndrePDF_$version.msi"
python3 -I "$work/make_update_manifest.py" --version "$version" --msi "$work/AndrePDF_$version.msi" --url "$url" --changelog "$work/CHANGELOG.md" > "$work/andre-pdf-update.json"
openssl dgst -sha256 -sign "$key" "$work/andre-pdf-update.json" > "$work/signature.der"
base64 -w0 "$work/signature.der" > "$work/andre-pdf-update.json.sig"
openssl dgst -sha256 -verify "$work/release-key.pub.pem" -signature "$work/signature.der" "$work/andre-pdf-update.json" >/dev/null \
    || die "the signature does not verify with the public key of the program"

python3 -I - "$version" "$work/CHANGELOG.md" > "$work/notes.md" <<'NOTES'
import sys, re
version, path = sys.argv[1], sys.argv[2]
text = open(path, encoding='utf-8').read()
m = re.search(r'^## ' + re.escape(version) + r'(?![\d.])[^\n]*$(.*?)(?=^## |\Z)', text, re.S | re.M)
print(m.group(1).strip() if m else '')
print('\n---\nInstall: run the .msi (not code-signed: SmartScreen -> More info -> Run anyway). Andre PDF updates itself from this release (signed manifest).')
NOTES

summary=$(python3 -I -c 'import json, sys; m = json.load(open(sys.argv[1], encoding="utf-8")); print(next(r["summary"] for r in m["releases"] if r["version"] == m["version"]))' "$work/andre-pdf-update.json")
echo "andre-release: about to publish v$version"
echo "  commit  $sha (run $run, on origin/master)"
echo "  MSI     SHA-256 $(sha256sum "$work/AndrePDF_$version.msi" | cut -d' ' -f1)"
echo "  summary $summary"
read -r -p "Publish this release? Type yes: " answer
[ "$answer" = yes ] || die "not published"

gh release create "v$version" -R "$repo" --target "$sha" --title "Andre PDF $version" --notes-file "$work/notes.md" \
    "$work/AndrePDF_$version.msi" "$work/andre-pdf-update.json" "$work/andre-pdf-update.json.sig"
echo "andre-release: v$version published from $sha (run $run)"
