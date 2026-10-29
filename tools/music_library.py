#!/usr/bin/env python3
"""Kernix PC library: bulk import, stable IDs, playlists and incremental SD sync."""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3
import struct
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[1]
MARKER = {'format': 'kernix-music', 'version': 1}
RUNTIME = '/mnt/music'

def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(4*1024*1024),b''): h.update(block)
    return h.hexdigest()

def text_frame(data):
    if not data:return ''
    enc={0:'latin1',1:'utf-16',2:'utf-16-be',3:'utf-8'}.get(data[0])
    if not enc:return ''
    return data[1:].decode(enc,errors='replace').strip('\x00 \r\n').replace('\x00',' / ')

def metadata(path):
    artist,title=(path.stem.split(' - ',1) if ' - ' in path.stem else ('Unknown Artist',path.stem))
    result={'title':title.strip(),'artist':artist.strip(),'album':''}
    with path.open('rb') as f:
        head=f.read(10)
        if head[:3]==b'ID3' and head[3] in (3,4):
            length=sum((b&127)<<(7*(3-i)) for i,b in enumerate(head[6:10]))
            data=f.read(min(length,16*1024*1024));offset=0
            if head[5]&0x40 and len(data)>=4:
                n=int.from_bytes(data[:4],'big') if head[3]==3 else sum((b&127)<<(7*(3-i)) for i,b in enumerate(data[:4]))
                offset=n+4 if head[3]==3 else n
            while offset+10<=len(data):
                name=data[offset:offset+4];size=data[offset+4:offset+8]
                length=int.from_bytes(size,'big') if head[3]==3 else sum((b&127)<<(7*(3-i)) for i,b in enumerate(size))
                if not length or offset+10+length>len(data):break
                key={b'TIT2':'title',b'TPE1':'artist',b'TALB':'album'}.get(name)
                if key:
                    value=text_frame(data[offset+10:offset+10+length])
                    if value:result[key]=value
                offset+=10+length
    return result

class Library:
    def __init__(self,home=ROOT/'library'):
        self.home=Path(home);self.home.mkdir(parents=True,exist_ok=True)
        (self.home/'tracks').mkdir(exist_ok=True)
        self.path=self.home/'catalog.json'
        self.data=json.loads(self.path.read_text()) if self.path.exists() else {'version':1,'tracks':{},'playlists':[]}
        if self.data.get('version')!=1:raise ValueError('Unsupported PC catalog version')
    def save(self):
        temp=self.path.with_suffix('.tmp')
        with temp.open('w') as f:
            json.dump(self.data,f,indent=2,ensure_ascii=False);f.write('\n');f.flush();os.fsync(f.fileno())
        os.replace(temp,self.path)
    def playlist(self,name):
        name=name.strip()
        if not name:raise ValueError('Playlist name cannot be empty')
        for p in self.data['playlists']:
            if p['name'].casefold()==name.casefold():return p
        used={p['id'] for p in self.data['playlists']}
        pid=next((f'{n:02d}' for n in range(1,100) if f'{n:02d}' not in used),None)
        if not pid:raise ValueError('All 99 playlist slots are in use')
        p={'id':pid,'name':name,'songs':[]};self.data['playlists'].append(p);return p
    def import_files(self,files,playlist=None):
        paths=[]
        for name in files:
            p=Path(name)
            paths.extend(sorted(p.rglob('*'))) if p.is_dir() else paths.append(p)
        added=[]
        by_hash={t['sha256']:sid for sid,t in self.data['tracks'].items()}
        target=self.playlist(playlist) if playlist else None
        for path in paths:
            if not path.is_file() or path.suffix.lower()!='.mp3':continue
            if not path.stat().st_size:raise ValueError(f'Empty audio file: {path}')
            digest=sha(path);sid=by_hash.get(digest)
            if not sid:
                n=max((int(v) for v in self.data['tracks']),default=0)+1
                if n>99999:raise ValueError('All 99999 song IDs are in use')
                sid=f'{n:05d}';destination=self.home/'tracks'/(digest+'.mp3')
                if not destination.exists():
                    temp=destination.with_suffix('.tmp');shutil.copyfile(path,temp)
                    if sha(temp)!=digest:temp.unlink();raise ValueError('Input changed while copying: '+str(path))
                    os.replace(temp,destination)
                self.data['tracks'][sid]={**metadata(path),'sha256':digest,'original_name':path.name}
                by_hash[digest]=sid
            if sid not in added:added.append(sid)
            if target and sid not in target['songs']:target['songs'].append(sid)
        self.save();return added
    def validate(self):
        for sid,t in self.data['tracks'].items():
            if len(sid)!=5 or not sid.isascii() or not sid.isdigit() or int(sid)==0:raise ValueError('Invalid song ID')
            p=self.home/'tracks'/(t['sha256']+'.mp3')
            if not p.is_file() or sha(p)!=t['sha256']:raise ValueError('Missing/changed track '+sid)
        for p in self.data['playlists']:
            if len(p['songs'])!=len(set(p['songs'])):raise ValueError('Duplicate membership in '+p['name'])
            for sid in p['songs']:
                if sid not in self.data['tracks']:raise ValueError('Unknown song '+sid)
    def database(self,path):
        with sqlite3.connect(path) as db:
            db.executescript('''PRAGMA foreign_keys=ON;
CREATE TABLE playlists(id TEXT PRIMARY KEY,name TEXT NOT NULL);
CREATE TABLE tracks(id TEXT PRIMARY KEY,title TEXT NOT NULL,artist TEXT NOT NULL,album TEXT NOT NULL,file_path TEXT NOT NULL UNIQUE,sha256 TEXT NOT NULL UNIQUE,available INTEGER NOT NULL DEFAULT 1,duration_ms INTEGER NOT NULL DEFAULT 0);
CREATE TABLE playlist_tracks(playlist_id TEXT REFERENCES playlists(id),position INTEGER NOT NULL CHECK(position>0),song_id TEXT REFERENCES tracks(id),PRIMARY KEY(playlist_id,position),UNIQUE(playlist_id,song_id));
PRAGMA user_version=2;''')
            for sid,t in self.data['tracks'].items():
                db.execute('INSERT INTO tracks(id,title,artist,album,file_path,sha256) VALUES(?,?,?,?,?,?)',(sid,t['title'],t['artist'],t['album'],RUNTIME+'/tracks/'+t['sha256']+'.mp3',t['sha256']))
            for p in self.data['playlists']:
                db.execute('INSERT INTO playlists VALUES(?,?)',(p['id'],p['name']))
                db.executemany('INSERT INTO playlist_tracks VALUES(?,?,?)',[(p['id'],i,sid) for i,sid in enumerate(p['songs'],1)])
            if db.execute('PRAGMA integrity_check').fetchone()[0]!='ok':raise ValueError('Database integrity error')
    def sync(self,destination,initialize=False):
        self.validate();dest=Path(destination).resolve()
        if not dest.is_dir():raise ValueError('Select the mounted KERNIX_MUSIC partition')
        marker=dest/'.kernix-music'
        if not marker.exists():
            if not initialize:raise ValueError('Not a Kernix music partition (.kernix-music is missing)')
            if any(dest.iterdir()):raise ValueError('Initialize requires an empty directory')
            marker.write_text(json.dumps(MARKER)+'\n')
        if marker.is_symlink() or json.loads(marker.read_text())!=MARKER:raise ValueError('Invalid music partition marker')
        for folder in ['tracks','releases']:
            if (dest/folder).is_symlink():raise ValueError('Refusing symlinked data directory')
            (dest/folder).mkdir(exist_ok=True)
        missing=[]
        for t in self.data['tracks'].values():
            source=self.home/'tracks'/(t['sha256']+'.mp3');target=dest/'tracks'/source.name
            if target.is_symlink():raise ValueError('Refusing symlinked track')
            if not target.exists() or sha(target)!=t['sha256']:missing.append((source,target))
        needed=sum(p.stat().st_size for p,_ in missing)+4*1024*1024
        if shutil.disk_usage(dest).free<needed:raise ValueError('Not enough free space for this sync')
        for source,target in missing:
            temp=target.with_suffix('.copying')
            try:
                with source.open('rb') as src,temp.open('wb') as out:
                    shutil.copyfileobj(src,out);out.flush();os.fsync(out.fileno())
                if sha(temp)!=source.stem:raise ValueError('Copy checksum mismatch')
                os.replace(temp,target)
            finally:
                if temp.exists():temp.unlink()
        release=uuid.uuid4().hex
        folder=dest/'releases'/release;folder.mkdir()
        self.database(folder/'library.db')
        with (folder/'library.db').open('rb') as f:os.fsync(f.fileno())
        # Persist new file names and the complete database before publication.
        for directory in [dest/'tracks',folder,dest/'releases']:
            fd=os.open(directory,os.O_RDONLY)
            try:os.fsync(fd)
            finally:os.close(fd)
        # A single atomic symlink publication selects complete database/files.
        link=dest/('.current-'+release);link.symlink_to('releases/'+release)
        if (dest/'current').exists() and not (dest/'current').is_symlink():raise ValueError('current must be a release symlink')
        os.replace(link,dest/'current')
        for directory in [dest/'tracks',folder,dest/'releases',dest]:
            fd=os.open(directory,os.O_RDONLY)
            try:os.fsync(fd)
            finally:os.close(fd)
        return len(missing),len(self.data['tracks'])

def gui(library):
    import tkinter as tk
    from tkinter import ttk,filedialog,messagebox,simpledialog
    app=tk.Tk();app.title('Kernix Music Library');app.geometry('1000x620')
    toolbar=ttk.Frame(app,padding=10);toolbar.pack(fill='x')
    choice=tk.StringVar(value='All songs');combo=ttk.Combobox(toolbar,textvariable=choice,state='readonly',width=25);combo.pack(side='left')
    tree=ttk.Treeview(app,columns=('id','title','artist','album'),show='headings',selectmode='extended')
    for col,width in [('id',75),('title',350),('artist',230),('album',230)]:tree.heading(col,text=col.title());tree.column(col,width=width)
    tree.pack(fill='both',expand=True,padx=10)
    status=tk.StringVar(value='Import MP3 files/folders, edit metadata, assign playlists, then sync the mounted SD music partition.')
    ttk.Label(app,textvariable=status,padding=10,wraplength=950).pack(fill='x')
    def playlist():return next((p for p in library.data['playlists'] if p['name']==choice.get()),None)
    def refresh():
        combo['values']=['All songs']+[p['name'] for p in library.data['playlists']]
        tree.delete(*tree.get_children());p=playlist()
        for sid in p['songs'] if p else sorted(library.data['tracks']):
            t=library.data['tracks'][sid];tree.insert('',tk.END,iid=sid,values=(sid,t['title'],t['artist'],t['album']))
    def run(fn):
        try:fn();refresh()
        except Exception as e:messagebox.showerror('Kernix',str(e))
    def import_music(folder=False):
        files=[filedialog.askdirectory()] if folder else filedialog.askopenfilenames(filetypes=[('MP3 audio','*.mp3 *.MP3')])
        files=[v for v in files if v]
        if files:
            ids=library.import_files(files,choice.get() if playlist() else None);status.set(f'Imported/recognized {len(ids)} songs. Exact duplicate files are stored once.')
    def create_playlist():
        name=simpledialog.askstring('New playlist','Playlist name:')
        if name:library.playlist(name);library.save();choice.set(name)
    def assign():
        ids=tree.selection()
        if not ids:return
        name=simpledialog.askstring('Add to playlist','Existing or new playlist name:')
        if name:
            p=library.playlist(name)
            for sid in ids:
                if sid not in p['songs']:p['songs'].append(sid)
            library.save();status.set(f'Added {len(ids)} songs to {p["name"]}.')
    def edit():
        ids=tree.selection()
        if len(ids)!=1:messagebox.showinfo('Edit metadata','Select one song.');return
        t=library.data['tracks'][ids[0]];win=tk.Toplevel(app);win.title('Edit song '+ids[0]);fields={}
        for row,key in enumerate(['title','artist','album']):
            ttk.Label(win,text=key.title()).grid(row=row,column=0,padx=10,pady=8)
            v=tk.StringVar(value=t[key]);ttk.Entry(win,textvariable=v,width=65).grid(row=row,column=1,padx=10);fields[key]=v
        def save():
            if not fields['title'].get().strip():return
            for k,v in fields.items():t[k]=v.get().strip()
            library.save();win.destroy();refresh()
        ttk.Button(win,text='Save',command=save).grid(row=3,column=1,pady=10)
    def move(delta):
        p=playlist();selected=tree.selection()
        if not p or len(selected)!=1:return
        i=p['songs'].index(selected[0]);j=i+delta
        if 0<=j<len(p['songs']):p['songs'][i],p['songs'][j]=p['songs'][j],p['songs'][i];library.save();refresh();tree.selection_set(selected)
    def remove():
        p=playlist()
        if p:
            selected=set(tree.selection());p['songs']=[v for v in p['songs'] if v not in selected];library.save()
    def sync():
        destination=filedialog.askdirectory(title='Select mounted KERNIX_MUSIC partition')
        if destination:
            copied,total=library.sync(destination);status.set(f'Sync complete: {copied} files copied, {total} songs total. Safely eject the card before booting the Pi.')
    for label,fn in [('Import files',lambda:import_music()),('Import folder',lambda:import_music(True)),('New playlist',create_playlist),('Add to playlist',assign),('Edit metadata',edit),('Sync to SD',sync)]:
        ttk.Button(toolbar,text=label,command=lambda f=fn:run(f)).pack(side='left',padx=3)
    bottom=ttk.Frame(app,padding=10);bottom.pack(fill='x')
    for label,fn in [('Move up',lambda:move(-1)),('Move down',lambda:move(1)),('Remove from playlist',remove)]:ttk.Button(bottom,text=label,command=lambda f=fn:run(f)).pack(side='left',padx=4)
    combo.bind('<<ComboboxSelected>>',lambda e:refresh());tree.bind('<Double-1>',lambda e:edit());refresh();app.mainloop()

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--library',type=Path,default=ROOT/'library')
    sub=parser.add_subparsers(dest='command',required=True)
    sub.add_parser('gui');sub.add_parser('list')
    imp=sub.add_parser('import');imp.add_argument('files',nargs='+');imp.add_argument('--playlist')
    sync=sub.add_parser('sync');sync.add_argument('destination',type=Path)
    stage=sub.add_parser('stage');stage.add_argument('destination',type=Path)
    args=parser.parse_args();library=Library(args.library)
    if args.command=='gui':gui(library)
    elif args.command=='list':print(json.dumps(library.data,indent=2,ensure_ascii=False))
    elif args.command=='import':print('Songs:',', '.join(library.import_files(args.files,args.playlist)))
    elif args.command=='sync':print('Files copied, total songs:',library.sync(args.destination))
    else:
        args.destination.mkdir(parents=True,exist_ok=True)
        print('Files staged, total songs:',library.sync(args.destination,initialize=True))
if __name__=='__main__':main()
