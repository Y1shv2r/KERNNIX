import os
import pathlib
import subprocess
import tempfile
import unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class AudioTests(unittest.TestCase):
    def test_backend_with_fake_decoder_and_output(self):
        with tempfile.TemporaryDirectory(prefix='kernix-audio-tests-') as tmp:
            folder=pathlib.Path(tmp)
            binary=folder/'backend-test'
            subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-o',str(binary),str(ROOT/'tests/test_backend.c'),'-pthread'],check=True)
            helper=folder/'output'
            helper.write_text('#!/bin/sh\nsleep 0.2\ncase "$1" in pair|pair-status) [ ! -e "$TEST_PAIR_FAIL_FILE" ]; exit $? ;; esac\n[ ! -e "$TEST_FAIL_FILE" ]\n')
            helper.chmod(0o755)
            env=dict(os.environ,KERNIX_MPG123=str(ROOT/'tests/fake-mpg123.py'),KERNIX_AUDIO_HELPER=str(helper),
                     TEST_PAIR_FAIL_FILE=str(folder/'pair-fail'),TEST_AUDIO_LOG=str(folder/'commands'),TEST_EOF_FILE=str(folder/'eof'),TEST_FAIL_FILE=str(folder/'fail'))
            result=subprocess.run([str(binary)],env=env,capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('JUMP 42', (folder/'commands').read_text())
if __name__=='__main__':unittest.main()
