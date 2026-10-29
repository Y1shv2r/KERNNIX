#!/usr/bin/python3
"""Private-bus BlueZ test double for the native TFT pairing agent."""
import os
from pathlib import Path
import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib
DBusGMainLoop(set_as_default=True)
bus=dbus.SystemBus()
name=dbus.service.BusName('org.bluez',bus)
mode=os.environ.get('PAIR_TEST_MODE','success')
paired=mode=='already'
owner=None
agent_path=None
log=Path(os.environ['PAIR_TEST_LOG'])
def record(text):
    with log.open('a') as f:f.write(text+'\n')
class Manager(dbus.service.Object):
    @dbus.service.method('org.bluez.AgentManager1',in_signature='os',out_signature='',sender_keyword='sender')
    def RegisterAgent(self,path,capability,sender=None):
        global owner,agent_path
        owner=sender;agent_path=path;record('register '+capability)
    @dbus.service.method('org.bluez.AgentManager1',in_signature='o',out_signature='')
    def RequestDefaultAgent(self,path):record('default')
    @dbus.service.method('org.bluez.AgentManager1',in_signature='o',out_signature='')
    def UnregisterAgent(self,path):record('unregister')
class Adapter(dbus.service.Object):
    @dbus.service.method('org.freedesktop.DBus.Properties',in_signature='ssv',out_signature='')
    def Set(self,iface,key,value):record('adapter '+key)
    @dbus.service.method('org.bluez.Adapter1',in_signature='',out_signature='')
    def StartDiscovery(self):record('scan start')
    @dbus.service.method('org.bluez.Adapter1',in_signature='',out_signature='')
    def StopDiscovery(self):record('scan stop')
class Device(dbus.service.Object):
    @dbus.service.method('org.freedesktop.DBus.Properties',in_signature='ss',out_signature='v')
    def Get(self,iface,key):
        if key=='Paired':return dbus.Boolean(paired)
        return dbus.String('AA:BB:CC:DD:EE:FF')
    @dbus.service.method('org.freedesktop.DBus.Properties',in_signature='ssv',out_signature='')
    def Set(self,iface,key,value):record('device '+key)
    @dbus.service.method('org.bluez.Device1',in_signature='',out_signature='',async_callbacks=('ok','error'))
    def Pair(self,ok,error):
        record('pair')
        if mode=='timeout':return
        if mode=='failure':
            error(dbus.exceptions.DBusException('Rejected',name='org.bluez.Error.AuthenticationFailed'));return
        agent=bus.get_object(owner,agent_path,introspect=False)
        path='/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF'
        if mode=='foreign':path='/org/bluez/hci0/dev_00_11_22_33_44_55'
        def accepted():
            global paired
            paired=True;record('authorized');ok()
        agent.AuthorizeService(dbus.ObjectPath(path),'0000110d-0000-1000-8000-00805f9b34fb',
                               dbus_interface='org.bluez.Agent1',reply_handler=accepted,error_handler=error)
    @dbus.service.method('org.bluez.Device1',in_signature='',out_signature='')
    def CancelPairing(self):record('cancel')
manager=Manager(bus,'/org/bluez')
adapter=Adapter(bus,'/org/bluez/hci0')
device=Device(bus,'/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF')
Path(os.environ['PAIR_TEST_READY']).touch()
GLib.MainLoop().run()
