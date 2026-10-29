import importlib.util
from pathlib import Path
import sqlite3
import subprocess
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('music_manager',ROOT/'tools/music_library.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class MusicManagerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build=tempfile.TemporaryDirectory();cls.scanner=Path(cls.build.name)/'scan'
        subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-o',str(cls.scanner),str(ROOT/'ui/library-scan.c'),str(ROOT/'ui/library.c'),'-lsqlite3'],check=True)
    @classmethod
    def tearDownClass(cls):cls.build.cleanup()
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.library=module.Library(self.root/'library')
        self.song=self.root/'Original song.mp3'
        text=b'\x03Tagged title';frame=b'TIT2'+len(text).to_bytes(4,'big')+b'\x00\x00'+text
        n=len(frame);size=bytes([(n>>21)&127,(n>>14)&127,(n>>7)&127,n&127])
        self.song.write_bytes(b'ID3\x03\x00\x00'+size+frame+b'fake audio payload')
        self.dest=self.root/'sd';self.dest.mkdir()
    def test_stable_ids_tags_and_shared_membership(self):
        self.assertEqual(self.library.import_files([self.song],'Metro'),['00001'])
        renamed=self.root/'Renamed song.mp3';renamed.write_bytes(self.song.read_bytes())
        self.assertEqual(self.library.import_files([renamed],'Daily'),['00001'])
        self.assertEqual(len(self.library.data['tracks']),1)
        self.assertEqual(self.library.data['tracks']['00001']['title'],'Tagged title')
        self.assertEqual([p['songs'] for p in self.library.data['playlists']],[['00001'],['00001']])
        self.assertEqual(len(list((self.library.home/'tracks').glob('*.mp3'))),1)
    def test_sync_and_native_v2_loader(self):
        self.library.import_files([self.song],'Metro');self.library.import_files([self.song],'Daily')
        self.assertEqual(self.library.sync(self.dest,initialize=True),(1,1))
        db=self.dest/'current/library.db'
        result=subprocess.run([str(self.scanner),'--validate',str(db)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('2 playlists, 2 memberships',result.stdout)
        track=next((self.dest/'tracks').glob('*.mp3'));stamp=track.stat().st_mtime_ns
        self.assertEqual(self.library.sync(self.dest),(0,1))
        self.assertEqual(track.stat().st_mtime_ns,stamp)
    def test_empty_playlist_and_large_membership_load(self):
        self.library.import_files([self.song],'Metro')
        self.library.playlist('Empty');self.library.save()
        self.library.sync(self.dest,initialize=True)
        result=subprocess.run([str(self.scanner),'--validate',str(self.dest/'current/library.db')],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('2 playlists, 1 memberships',result.stdout)

    def test_sync_failure_keeps_previous_release(self):
        self.library.import_files([self.song],'Metro');self.library.sync(self.dest,initialize=True)
        previous=(self.dest/'current').readlink()
        with patch.object(module.shutil,'disk_usage',return_value=type('Disk',(),{'free':0})()):
            with self.assertRaises(ValueError):self.library.sync(self.dest)
        self.assertEqual((self.dest/'current').readlink(),previous)
        asset=next((self.library.home/'tracks').glob('*.mp3'));asset.write_bytes(b'corrupt')
        with self.assertRaises(ValueError):self.library.sync(self.dest)
        self.assertEqual((self.dest/'current').readlink(),previous)
    def test_wrong_destination_and_symlinks_rejected(self):
        with self.assertRaises(ValueError):self.library.sync(self.dest)
        self.library.sync(self.dest,initialize=True)
        (self.dest/'tracks').rmdir();(self.dest/'tracks').symlink_to(self.root/'library/tracks')
        with self.assertRaises(ValueError):self.library.sync(self.dest)
    def test_source_names_unchanged_and_metadata_edit(self):
        self.library.import_files([self.song],'Metro')
        self.assertTrue(self.song.exists())
        self.library.data['tracks']['00001']['title']='My edited title';self.library.save()
        self.library.sync(self.dest,initialize=True)
        with sqlite3.connect(self.dest/'current/library.db') as db:
            self.assertEqual(db.execute('SELECT title FROM tracks').fetchone()[0],'My edited title')
if __name__=='__main__':unittest.main()
