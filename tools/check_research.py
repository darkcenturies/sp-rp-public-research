"""Validate public archive completeness, exact bytes and target coverage."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parent.parent
archive = root / 'docs/reverse-engineering/generated'
ledger = json.loads((archive.parent / 'checksums.json').read_text(encoding='utf-8-sig'))
listed = set()
for entry in ledger['files']:
    name = entry['path']
    p = (root / name).resolve()
    if not p.is_relative_to(archive.resolve()) and p != (archive.parent / 'ssmp-server-1.3.1-patch1.pseudo.cpp').resolve():
        raise SystemExit('Unexpected archive path: ' + name)
    if name in listed:
        raise SystemExit('Duplicate archive entry: ' + name)
    listed.add(name)
    data = p.read_bytes()
    if len(data) != entry['bytes'] or hashlib.sha256(data).hexdigest() != entry['sha256']:
        raise SystemExit('Archive checksum mismatch: ' + name)
actual = {p.relative_to(root).as_posix() for p in archive.rglob('*') if p.is_file()}
actual.add('docs/reverse-engineering/ssmp-server-1.3.1-patch1.pseudo.cpp')
if actual != listed:
    raise SystemExit('Archive inventory mismatch: ' + str(sorted(actual ^ listed)))
index = json.loads((archive / 'index.json').read_text(encoding='utf-8-sig'))
targets = {t['id']: t for t in index['targets']}
if len(targets) != len(index['targets']) or set(targets) != {p.name for p in archive.iterdir() if p.is_dir()}:
    raise SystemExit('Target inventory mismatch')
for target_id, target in targets.items():
    folder = archive / target_id
    for filename in ['decompiled.c', 'disassembly.txt', 'functions.csv', 'calls.csv', 'strings.csv', 'symbols.csv', 'metadata.json']:
        if not (folder / filename).is_file():
            raise SystemExit(target_id + ': missing ' + filename)
    metadata = json.loads((folder / 'metadata.json').read_text(encoding='utf-8-sig'))
    for key in ['sha256', 'functions_total', 'functions_internal', 'functions_decompiled', 'functions_failed', 'strings', 'symbols']:
        if metadata[key] != target[key]:
            raise SystemExit(target_id + ': mismatched ' + key)
    if metadata['functions_decompiled'] + metadata['functions_failed'] != metadata['functions_internal']:
        raise SystemExit(target_id + ': inconsistent coverage')
print(f'Archive verified: {len(targets)} targets, {len(listed)} files, exact SHA-256 and metadata.')
