"""Exercise AUX initialization without accessing sound hardware."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class StereoDefaultsTests(unittest.TestCase):
    def run_mixer(self, fail=''):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            mixer = path / 'amixer'
            mixer.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$MIXER_LOG"\n'
                             '[ -z "$FAIL_CONTROL" ] || [ "$5" != "$FAIL_CONTROL" ]\n')
            mixer.chmod(0o755)
            source = (ROOT / 'board/kernix/rootfs-overlay/usr/libexec/kernix/alsa').read_text()
            begin = source.index('wait_for_card() {')
            end = source.index('headphone_defaults() {')
            source = source[:begin] + 'wait_for_card() { return 0; }\n\n' + source[end:]
            script = path / 'alsa'
            script.write_text(source)
            log = path / 'calls'
            env = dict(os.environ, PATH=f'{path}:' + os.environ['PATH'],
                       MIXER_LOG=str(log), FAIL_CONTROL=fail)
            result = subprocess.run(['sh', str(script), 'start'], env=env, capture_output=True)
            return result.returncode, log.read_text().splitlines()

    def test_balanced_stereo_routing(self):
        status, calls = self.run_mixer()
        self.assertEqual(status, 0)
        for control in ['DAC 87,87', 'HP Analog 100,100', 'HP Driver 0,0 unmute',
                        'DAC Left Input Left Data', 'DAC Right Input Right Data',
                        'Output Left From Left DAC on', 'Output Right From Right DAC on',
                        'HP Left on', 'HP Right on']:
            self.assertIn('-q -c KernixDAC sset ' + control, calls)

    def test_failed_channel_setup_leaves_headphones_muted(self):
        status, calls = self.run_mixer('HP Analog')
        self.assertNotEqual(status, 0)
        self.assertTrue(calls[-1].endswith('HP Driver mute'))
        self.assertFalse(any('unmute' in call for call in calls))
