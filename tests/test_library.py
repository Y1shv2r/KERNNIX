"""Host integration tests; compile the real scanner and use disposable databases."""
import pathlib
import sqlite3
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class LibraryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='kernix-sql-tests-')
        cls.scanner = pathlib.Path(cls.build.name) / 'scan'
        subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-o', str(cls.scanner),
                        str(ROOT/'ui/library-scan.c'), str(ROOT/'ui/library.c'), '-lsqlite3'], check=True)

        cls.player = pathlib.Path(cls.build.name) / 'player-test'
        subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-o', str(cls.player),
                        str(ROOT/'tests/test_player_library.c'), str(ROOT/'ui/library.c'), str(ROOT/'ui/backend.c'), '-lsqlite3', '-pthread'], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='kernix-library-')
        self.addCleanup(self.temp.cleanup)
        self.folder = pathlib.Path(self.temp.name)/'music'
        self.folder.mkdir()
        self.db = pathlib.Path(self.temp.name)/'library.db'

    def scan(self, success=True):
        result = subprocess.run([str(self.scanner), str(self.folder), str(self.db)], capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def query(self, sql, args=()):
        with sqlite3.connect(self.db) as db:
            return db.execute(sql, args).fetchall()

    def test_ids_order_metadata_preserved_and_missing_restored(self):
        for name in ['0305.mp3','0102.mp3','0101.mp3','9912.MP3','0001.mp3','0100.mp3','bad.mp3']:
            (self.folder/name).write_bytes(b'fixture')
        self.scan()
        self.assertEqual(self.query('SELECT id,playlist_id,position FROM tracks ORDER BY id'),
                         [('0101','01',1),('0102','01',2),('0305','03',5),('9912','99',12)])
        with sqlite3.connect(self.db) as db:
            db.execute("UPDATE playlists SET name='My playlist' WHERE id='03'")
            db.execute("UPDATE tracks SET title='My song',artist='My artist',album='My album' WHERE id='0305'")
        self.scan()
        self.assertEqual(self.query("SELECT title,artist,album FROM tracks WHERE id='0305'"), [('My song','My artist','My album')])
        self.assertEqual(self.query("SELECT name FROM playlists WHERE id='03'"), [('My playlist',)])
        (self.folder/'0305.mp3').unlink()
        self.scan()
        self.assertEqual(self.query("SELECT available,title FROM tracks WHERE id='0305'"), [(0,'My song')])
        (self.folder/'0305.mp3').write_bytes(b'fixture')
        self.scan()
        self.assertEqual(self.query("SELECT available,title FROM tracks WHERE id='0305'"), [(1,'My song')])
        self.assertEqual(self.query('PRAGMA integrity_check'), [('ok',)])
        self.assertEqual(self.query('PRAGMA foreign_key_check'), [])

    def test_duplicate_rolls_back_scan(self):
        (self.folder/'0101.mp3').write_bytes(b'fixture')
        self.scan()
        (self.folder/'0101.MP3').write_bytes(b'fixture')
        self.assertIn('Duplicate track ID', self.scan(False).stderr)
        self.assertEqual(self.query('SELECT id,available FROM tracks'), [('0101',1)])

    def test_missing_folder_preserves_database(self):
        (self.folder/'0101.mp3').write_bytes(b'fixture')
        self.scan()
        self.folder.rename(self.folder.with_name('away'))
        self.scan(False)
        self.assertEqual(self.query('SELECT available FROM tracks'), [(1,)])

    def test_empty_library_and_unknown_schema(self):
        self.assertIn('0 playlists, 0 tracks', self.scan().stdout)
        with sqlite3.connect(self.db) as db:
            db.execute('PRAGMA user_version=99')
        self.assertIn('Unsupported library schema', self.scan(False).stderr)

    def test_database_backed_ui_paging(self):
        for playlist in range(1, 8):
            for song in range(1, 13):
                (self.folder/f'{playlist:02d}{song:02d}.mp3').write_bytes(b'fixture')
        self.scan()
        subprocess.run([str(self.player), str(self.db)], check=True, capture_output=True, text=True)

    def test_non_audio_files_are_ignored(self):
        (self.folder/'README.md').write_text('User-supplied music only')
        (self.folder/'0101.wav').write_bytes(b'not an MP3')
        self.scan()
        self.assertEqual(self.query('SELECT id FROM tracks'), [])

if __name__ == '__main__':
    unittest.main()
