"""Public source packaging must exclude the maintainer's local data."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('source_export', ROOT/'scripts/export-source.py')
export = importlib.util.module_from_spec(spec)
spec.loader.exec_module(export)


class SourceExportTests(unittest.TestCase):
    def test_export_excludes_local_state_but_keeps_required_assets(self):
        names = {path.relative_to(ROOT).as_posix() for path in export.source_files()}
        for prefix in ['output/', 'buildroot/', '.local/', 'music/', 'images/', 'dist/']:
            self.assertFalse(any(name.startswith(prefix) for name in names), prefix)
        self.assertEqual([name for name in names if name.startswith('library/')], ['library/README.md'])
        self.assertFalse(any('__pycache__' in name or name.endswith(('.mp3', '.db', '.pyc')) for name in names))
        for name in ['configs/buildroot-revision', 'board/kernix/cmdline.txt',
                     'ui/assets/crimson-idle.h', 'ui/assets/fonts/LICENSE',
                     'board/kernix/rootfs-overlay/etc/asound.conf']:
            self.assertIn(name, names)
        self.assertIn('BT_DEVICE=\n', (ROOT/'board/kernix/rootfs-overlay/etc/default/kernix-audio').read_text())

    def test_post_build_uses_only_explicit_local_override(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            external = path/'source'
            target = path/'target'
            destination = target/'etc/default/kernix-audio'
            destination.parent.mkdir(parents=True)
            destination.write_text('BT_DEVICE=\n')
            (target/'etc/init.d').mkdir()
            hook = ROOT/'board/kernix/post-build.sh'
            import os
            env = dict(os.environ, BR2_EXTERNAL_KERNIX_PATH=str(external))
            subprocess.run(['sh', str(hook), str(target)], env=env, check=True)
            self.assertEqual(destination.read_text(), 'BT_DEVICE=\n')
            (external/'.local').mkdir(parents=True)
            settings = 'BT_DEVICE=AA:BB:CC:DD:EE:FF\n'
            (external/'.local/kernix-audio').write_text(settings)
            subprocess.run(['sh', str(hook), str(target)], env=env, check=True)
            self.assertEqual(destination.read_text(), settings)
