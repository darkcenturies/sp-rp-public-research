"""Check tracked public files for prohibited paths and obvious credential formats."""
import ast
from pathlib import Path
import re
import subprocess
import sys
root = Path(__file__).resolve().parent.parent
paths = subprocess.check_output(['git', 'ls-files', '-z'], cwd=root).decode().split('\0')
errors = []
for name in filter(None, paths):
    p = root / name
    if name.startswith(('component/', 'deploy/radar-release/')) or p.name in {'radar3d.cpp', 'radar3d.h', 'main_radar3d.cpp', 'radarbox.h', 'radar_logo.h', 'router.cpp', 'router.h', 'vehgraph.hpp', '3D_RADAR_DEVELOPMENT.md', 'RADAR_REVIEW_2026-09-06.md', 'RADAR3D-STATUS.md', 'RADAR3D-OCCLUSION.md'} or name.startswith('deploy/world3d-'):
        errors.append(name + ': unreleased Radar work')
    if name.startswith(('gamemodes/', 'database/', 'scriptfiles/', 'server/', 'reference/')) or p.suffix.lower() in {'.sql', '.db', '.amx', '.idb', '.i64', '.pem', '.key'} or p.name.startswith('.env'):
        errors.append(name + ': private/vendor artifact path')
    data = p.read_bytes()
    size_limit = 100_000_000 if name.startswith('docs/reverse-engineering/generated/') else 40_000_000
    if len(data) > size_limit:
        errors.append(name + ': unexpectedly large file')
    if b'\0' in data[:8192]:
        continue
    text = data.decode('utf-8-sig', errors='replace')
    if re.search(r'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{50,}|-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----)', text):
        errors.append(name + ': possible credential')
    if p.suffix == '.py' and 'third-party/' not in name:
        try:
            ast.parse(text, filename=name)
        except SyntaxError as exc:
            errors.append(name + ': ' + str(exc))
if errors:
    raise SystemExit('\n'.join(errors))
subprocess.run([sys.executable, str(root / 'tools/check_research.py')], check=True, cwd=root)
print('Public boundary and Python syntax checks passed.')
