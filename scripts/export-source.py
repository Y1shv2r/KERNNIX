#!/usr/bin/env python3
"""Export maintained source without local builds, device settings or music."""
import argparse
import hashlib
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]
FILES = ['README.md', 'CONTRIBUTING.md', 'THIRD_PARTY_NOTICES.md', 'LICENSE',
         '.gitignore', '.gitattributes', 'Config.in', 'external.desc', 'external.mk']
DIRECTORIES = ['.github', 'board', 'configs', 'package', 'scripts', 'tests', 'tools', 'ui']


def source_files(root=ROOT):
    paths = [root / name for name in FILES if (root / name).is_file()]
    for directory in DIRECTORIES:
        for path in (root / directory).rglob('*'):
            relative = path.relative_to(root)
            if any(part in ('__pycache__', '.git', '.local') for part in relative.parts):
                continue
            if path.suffix.lower() in ('.pyc', '.mp3', '.wav', '.db', '.img', '.log'):
                continue
            if path.is_symlink():
                if relative.as_posix() != 'board/kernix/rootfs-overlay/etc/asound.conf':
                    raise ValueError('Unexpected source symlink: ' + str(relative))
                paths.append(path)
            elif path.is_file():
                paths.append(path)
    paths += [root / 'library/README.md']
    return sorted(set(path for path in paths if path.is_file() or path.is_symlink()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'dist/kernix-source.tar.gz')
    args = parser.parse_args()
    paths = source_files()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(args.output, 'w:gz', dereference=False) as archive:
        for path in paths:
            archive.add(path, arcname='kernix/' + path.relative_to(ROOT).as_posix(), recursive=False)
    checksum = hashlib.sha256(args.output.read_bytes()).hexdigest()
    args.output.with_name(args.output.name + '.sha256').write_text(checksum + '  ' + args.output.name + '\n')
    print(f'{args.output}: {len(paths)} source files, {args.output.stat().st_size} bytes')
    if not (ROOT / 'LICENSE').is_file():
        print('License selection is pending; this is a review bundle, not a licensed release.')


if __name__ == '__main__':
    main()
