import os
from pathlib import Path
import shlex
import signal
import subprocess
import tempfile
import time
import unittest
ROOT=Path(__file__).resolve().parents[1]
MAC='AA:BB:CC:DD:EE:FF'
class PairingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build=tempfile.TemporaryDirectory()
        cls.binary=Path(cls.build.name)/'pair'
        flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','dbus-1'],text=True))
        subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror',str(ROOT/'ui/bluetooth-pair.c'),'-o',str(cls.binary),*flags],check=True)
    @classmethod
    def tearDownClass(cls):cls.build.cleanup()
    def run_case(self,mode,action='pair',cancel=False):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            daemon=subprocess.Popen(['dbus-daemon','--session','--nofork','--print-address=1'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            address=daemon.stdout.readline().strip()
            if not address:
                _,error=daemon.communicate(timeout=5)
                self.fail('Cannot start private test bus: '+error)
            env=dict(os.environ,DBUS_SYSTEM_BUS_ADDRESS=address,PAIR_TEST_MODE=mode,
                     PAIR_TEST_LOG=str(root/'log'),PAIR_TEST_READY=str(root/'ready'))
            mock=subprocess.Popen(['/usr/bin/python3',str(ROOT/'tests/mock_bluez.py')],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            try:
                for _ in range(100):
                    if (root/'ready').exists():break
                    if mock.poll() is not None:self.fail(mock.stderr.read())
                    time.sleep(.02)
                self.assertTrue((root/'ready').exists())
                child=subprocess.Popen([str(self.binary),action,'hci0',MAC],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
                if cancel:
                    for _ in range(100):
                        if (root/'log').exists() and 'pair\n' in (root/'log').read_text():break
                        time.sleep(.02)
                    child.send_signal(signal.SIGTERM)
                out,err=child.communicate(timeout=10)
                log=(root/'log').read_text() if (root/'log').exists() else ''
                return child.returncode,log,out+err
            finally:
                mock.terminate();mock.communicate(timeout=5)
                daemon.terminate();daemon.communicate(timeout=5)
    def test_pair_authorize_trust_and_cleanup(self):
        code,log,output=self.run_case('success')
        self.assertEqual(code,0,output)
        for text in ['register NoInputNoOutput','authorized','device Trusted','scan stop','unregister']:self.assertIn(text,log)
    def test_existing_pairing_is_idempotent(self):
        code,log,output=self.run_case('already')
        self.assertEqual(code,0,output)
        self.assertNotIn('scan start',log)
        self.assertIn('device Trusted',log)
    def test_status_reads_real_pairing_state(self):
        self.assertEqual(self.run_case('already','status')[0],0)
        self.assertNotEqual(self.run_case('success','status')[0],0)
    def test_auth_failure_and_other_device_do_not_succeed(self):
        for mode in ['failure','foreign']:
            code,log,output=self.run_case(mode)
            self.assertNotEqual(code,0,output)
            self.assertNotIn('device Trusted',log)
            self.assertIn('scan stop',log)
    def test_cancellation_cleans_up_discovery_and_agent(self):
        code,log,output=self.run_case('timeout',cancel=True)
        self.assertNotEqual(code,0,output)
        for text in ['cancel','scan stop','unregister']:self.assertIn(text,log)
if __name__=='__main__':unittest.main()
