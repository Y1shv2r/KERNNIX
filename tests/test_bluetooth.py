"""Bluetooth selector regression tests without a controller or system D-Bus."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
OVERLAY = ROOT / 'board/kernix/rootfs-overlay'
MAC = 'AA:BB:CC:DD:EE:FF'
PCM = '/org/bluealsa/hci0/dev_AA_BB_CC_DD_EE_FF/a2dpsrc/sink'

class BluetoothTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / 'sys/hci0').mkdir(parents=True)
        (self.root / 'state').mkdir()
        (self.root / 'state/asound.conf').write_text('# Kernix output: dac\n')
        self.env = dict(os.environ, PATH=str(self.root)+':'+os.environ['PATH'],
                        TEST_LOG=str(self.root/'log'), TEST_COUNT=str(self.root/'count'))
        self.put('alsa', '#!/bin/sh\necho "alsa $*" >> "$TEST_LOG"\nexit "${ALSA_FAIL:-0}"\n')
        (self.root/'asound/card0').mkdir(parents=True)
        (self.root/'asound/card0/id').write_text('KernixDAC\n')
        (self.root/'audio-default').write_text('BT_DEVICE='+MAC+'\n')
        self.put('sleep', '#!/bin/sh\nexit 0\n')
        self.put('service', '#!/bin/sh\necho "service $*" >> "$TEST_LOG"\nexit "${SERVICE_FAIL:-0}"\n')
        self.put('dbus-send', '''#!/bin/sh
echo "dbus $*" >> "$TEST_LOG"
case "$*" in
 *ConnectProfile*) exit "${CONNECT_FAIL:-0}" ;;
 *Trusted*) exit "${TRUST_FAIL:-0}" ;;
esac
exit 0
''')
        self.put('bluealsa-cli', '''#!/bin/sh
echo "cli $*" >> "$TEST_LOG"
[ "${NO_PCM:-0}" = 0 ] || exit 1
if [ "$1" = list-pcms ]; then
 echo '/org/bluealsa/hci0/dev_AA_BB_CC_DD_EE_FF/a2dpsrc/sink'
 exit 0
fi
n=0; [ ! -f "$TEST_COUNT" ] || n=$(cat "$TEST_COUNT")
n=$((n + 1)); echo "$n" > "$TEST_COUNT"
[ "${DISAPPEAR:-0}" = 0 ] || [ "$n" -eq 1 ] || exit 1
[ "${DELAY_PCM:-0}" = 0 ] || [ "$n" -gt 2 ] || exit 1
printf 'Transport: %s\nMode: %s\n' "${TRANSPORT:-A2DP-source}" "${MODE:-sink}"
printf 'Channels: %s\nSampling: %s Hz\n' "${CHANNELS:-2}" "${RATE:-48000}"
''')
        source = (OVERLAY/'usr/libexec/kernix/bluetooth-connect').read_text()
        self.put('connect', source.replace('/usr/bin/kernix-audio-service', str(self.root/'service'))
                 .replace('/sys/class/bluetooth', str(self.root/'sys')))
        source = (OVERLAY/'usr/bin/kernix-audio').read_text()
        self.put('audio', source.replace('STATE_DIR=/var/lib/kernix-audio', 'STATE_DIR='+str(self.root/'state'))
                 .replace('/usr/libexec/kernix/bluetooth-connect', str(self.root/'connect'))
                 .replace('/usr/libexec/kernix/alsa', str(self.root/'alsa'))
                 .replace('/etc/default/kernix-audio', str(self.root/'audio-default'))
                 .replace('/proc/asound', str(self.root/'asound')))

    def put(self, name, content):
        p = self.root/name
        p.write_text(content)
        p.chmod(0o755)

    def select(self, **env):
        return subprocess.run([str(self.root/'audio'), 'bluetooth', MAC],
                              env=dict(self.env, **env), capture_output=True, text=True, timeout=5)

    def config(self):
        return (self.root/'state/asound.conf').read_text()

    def test_ready_endpoint_bypasses_bluetoothctl_text_and_reconnect(self):
        result = self.select()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('service "org.bluealsa"', self.config())
        self.assertIn('rate 48000', self.config())
        self.assertIn('channels 2', self.config())
        self.assertNotIn('ConnectProfile', (self.root/'log').read_text())
        self.assertIn(PCM, (self.root/'log').read_text())

    def test_async_endpoint_after_connect(self):
        result = self.select(DELAY_PCM='1')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('0000110b-0000-1000-8000-00805f9b34fb', (self.root/'log').read_text())

    def test_unavailable_or_wrong_endpoint_keeps_dac(self):
        for env in ({'NO_PCM':'1'}, {'TRANSPORT':'A2DP-sink'}, {'MODE':'source'},
                    {'DISAPPEAR':'1'}, {'SERVICE_FAIL':'1'}, {'TRUST_FAIL':'1'},
                    {'NO_PCM':'1', 'CONNECT_FAIL':'1'}, {'RATE':'0'}, {'RATE':'bad'}, {'CHANNELS':'8'}):
            with self.subTest(env=env):
                (self.root/'count').unlink(missing_ok=True)
                result = self.select(**env)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(self.config(), '# Kernix output: dac\n')

    def test_negotiated_rate_is_preserved_without_codec_selection(self):
        result = self.select(RATE='44100', CHANNELS='1')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('rate 44100', self.config())
        self.assertIn('channels 1', self.config())
        self.assertNotIn('SelectCodec', (self.root/'log').read_text())

    def test_single_commands_prepare_each_output(self):
        result = subprocess.run([str(self.root/'audio'), 'bluetooth'], env=self.env,
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('alsa stop', (self.root/'log').read_text())
        self.assertIn('output: bluetooth', self.config())
        result = subprocess.run([str(self.root/'audio'), 'aux'], env=self.env,
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('alsa start', (self.root/'log').read_text())
        self.assertIn('output: dac', self.config())

    def test_mixer_failure_does_not_commit_output(self):
        result = self.select(ALSA_FAIL='1')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.config(), '# Kernix output: dac\n')
        (self.root/'state/asound.conf').write_text('# Kernix output: bluetooth previous\n')
        result = subprocess.run([str(self.root/'audio'), 'aux'],
                                env=dict(self.env, ALSA_FAIL='1'), capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.config(), '# Kernix output: bluetooth previous\n')

    def test_bad_address_rejected_before_service_start(self):
        result = subprocess.run([str(self.root/'audio'), 'bluetooth', 'bad;address'],
                                env=self.env, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.root/'log').exists())

class DaemonTests(unittest.TestCase):
    def test_start_readiness_and_duplicate_handling(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            ready = root/'ready'
            log = root/'calls'
            env = dict(os.environ, PATH=d+':'+os.environ['PATH'],
                       READY_FILE=str(ready), CALL_LOG=str(log))
            scripts = {
                'bluealsa-cli': '#!/bin/sh\n[ -e "$READY_FILE" ]\n',
                'pidof': '#!/bin/sh\n[ "${EXISTING_PROCESS:-0}" = 1 ]\n',
                'sleep': '#!/bin/sh\nexit 0\n',
                'start-stop-daemon': '#!/bin/sh\necho "$*" >> "$CALL_LOG"\n[ "${FAIL_START:-0}" = 0 ] || exit 1\ntouch "$READY_FILE"\n',
            }
            for name, text in scripts.items():
                p = root/name
                p.write_text(text)
                p.chmod(0o755)
            service = root/'service'
            service.write_text((OVERLAY/'usr/libexec/kernix/bt-daemon').read_text()
                               .replace('/run/', d+'/').replace('/tmp/', d+'/'))
            service.chmod(0o755)
            def start(**extra):
                return subprocess.run([str(service), 'bluealsa', 'start'],
                                      env=dict(env, **extra), capture_output=True, text=True)
            self.assertEqual(start().returncode, 0)
            calls = log.read_text()
            self.assertIn('--sbc-quality=low', calls)
            self.assertEqual(start().returncode, 0)
            self.assertEqual(log.read_text(), calls, 'ready service must not spawn twice')
            ready.unlink()
            self.assertNotEqual(start(EXISTING_PROCESS='1').returncode, 0)
            self.assertEqual(log.read_text(), calls, 'unready existing daemon must not be duplicated')
            self.assertNotEqual(start(FAIL_START='1').returncode, 0)

if __name__ == '__main__':
    unittest.main()
