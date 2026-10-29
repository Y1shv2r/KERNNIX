#!/usr/bin/python3
"""Remote-protocol test double: never opens an audio device."""
import os
import select
import sys
import time
playing = False
pending = b""
print('@R MPG123 TEST', flush=True)
while True:
    ready, _, _ = select.select([sys.stdin], [], [], 0.02)
    if ready:
        data = os.read(0, 8192)
        if not data:
            break
        pending += data
        while b'\n' in pending:
            raw, pending = pending.split(b'\n', 1)
            command = raw.decode() + '\n'
            with open(os.environ['TEST_AUDIO_LOG'], 'a') as log:
                log.write(command)
            if command.startswith('LOADPAUSED '):
                playing = False
                print('@P 1', flush=True)
            elif command.startswith('JUMP '):
                print('@J 42', flush=True)
            elif command.strip() == 'PAUSE':
                playing = not playing
                print('@P 2' if playing else '@P 1', flush=True)
                if playing:
                    print('@F 42 200 12.0 48.0', flush=True)
    if playing and os.path.exists(os.environ['TEST_EOF_FILE']):
        os.unlink(os.environ['TEST_EOF_FILE'])
        playing = False
        print('@P 3\n@P 0', flush=True)
