#!/usr/bin/env python3
"""Keep independent, checksummed SD images; never flash or rebuild them."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = ROOT / 'dist/theme-images'

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    save = commands.add_parser('save')
    save.add_argument('theme')
    save.add_argument('--image', type=Path, default=ROOT/'output/images/sdcard.img')
    save.add_argument('--note', default='Hardware validation not recorded.')
    save.add_argument('--preview', type=Path)
    commands.add_parser('list')
    args = parser.parse_args()
    if args.command == 'list':
        for manifest in sorted(ARCHIVE.glob('*/manifest.json')):
            entry = json.loads(manifest.read_text())
            print(f"{entry['theme']} | {entry['saved_utc']} | {manifest.parent/'sdcard.img'}")
            print('  '+entry['note'])
        return
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]*', args.theme):
        parser.error('Use a lowercase theme name containing letters, digits and hyphens.')
    image = args.image.resolve(strict=True)
    if not image.is_file() or image.stat().st_size == 0:
        parser.error('Image must be a nonempty regular file.')
    checksum = digest(image)
    for existing in ARCHIVE.glob('*/manifest.json'):
        entry = json.loads(existing.read_text())
        if entry['sha256'] == checksum and entry['theme'] == args.theme:
            if digest(existing.parent/'sdcard.img') != checksum:
                raise RuntimeError('Existing archive checksum mismatch: '+str(existing.parent))
            print('Already archived: '+str(existing.parent))
            return
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    folder = ARCHIVE/f'{args.theme}-{stamp}-{checksum[:12]}'
    folder.mkdir(parents=True, exist_ok=False)
    try:
        target = folder/'sdcard.img'
        shutil.copyfile(image, target)
        if digest(target) != checksum or digest(image) != checksum:
            raise RuntimeError('Image changed or copy verification failed.')
        entry = dict(theme=args.theme, saved_utc=stamp, source=str(image),
                     sha256=checksum, bytes=target.stat().st_size, note=args.note)
        (folder/'SHA256SUMS').write_text(checksum+'  sdcard.img\n')
        if args.preview:
            shutil.copyfile(args.preview, folder/'preview.png')
        (folder/'manifest.json').write_text(json.dumps(entry, indent=2)+'\n')
        target.chmod(0o444)
        print(folder)
    except BaseException:
        shutil.rmtree(folder)
        raise

if __name__ == '__main__':
    main()
