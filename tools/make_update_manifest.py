#!/usr/bin/env python3
# Andre PDF: writes the update manifest of a release from CHANGELOG.md (the same
# format PDFVersionHistory reads) and the installer. Run by andre-release.sh.
import argparse, hashlib, json, os, re, sys

def parse_changelog(text):
    releases = []
    heading = re.compile(r'^##\s+(\d[\d.]*)\s*(?:-\s*(\d{4}-\d{2}-\d{2}))?\s*$')
    for raw in text.splitlines():
        line = raw.strip()
        m = heading.match(line)
        if m:
            releases.append({'version': m.group(1), 'date': m.group(2) or '', 'summary': '', 'changes': []})
            continue
        if not releases:
            continue
        if line.startswith('Summary:') and not releases[-1]['summary']:
            releases[-1]['summary'] = line[len('Summary:'):].strip()
        elif re.match(r'^[-*]\s+', line):
            releases[-1]['changes'].append(re.sub(r'^[-*]\s+', '', line))
        elif line and releases[-1]['changes'] and raw.startswith('  '):
            releases[-1]['changes'][-1] += ' ' + line
    return releases

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--version', required=True)
    p.add_argument('--msi', required=True)
    p.add_argument('--url', required=True)
    p.add_argument('--changelog', required=True)
    a = p.parse_args()
    if not re.fullmatch(r'\d{1,5}(\.\d{1,5}){0,3}', a.version) or not a.url.startswith('https://github.com/'):
        sys.exit('invalid version or url')
    releases = parse_changelog(open(a.changelog, encoding='utf-8').read())
    newest = next((r for r in releases if r['version'] == a.version), None)
    if not newest or not newest['summary'] or not newest['changes'] or not newest['date']:
        sys.exit(f'CHANGELOG.md has no complete entry for {a.version} (date, Summary:, changes)')
    data = open(a.msi, 'rb').read()
    manifest = {
        'product': 'Andre PDF',
        'version': a.version,
        'releases': releases,
        'windows': {'url': a.url, 'sha256': hashlib.sha256(data).hexdigest(), 'size': len(data)},
    }
    sys.stdout.write(json.dumps(manifest, ensure_ascii=False, indent=1))

if __name__ == '__main__':
    main()
