#define _POSIX_C_SOURCE 200809L
#include <dbus/dbus.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AGENT "/org/kernix/agent"
static volatile sig_atomic_t stopped;
static char device[160];
static void stop(int sig) { (void)sig; stopped=1; }
static DBusMessage *request(const char *path,const char *iface,const char *method) {
 return dbus_message_new_method_call("org.bluez",path,iface,method);
}
static DBusMessage *send_call(DBusConnection *bus,DBusMessage *msg) {
 if(!msg)return NULL;
 DBusError err=DBUS_ERROR_INIT;
 DBusMessage *reply=dbus_connection_send_with_reply_and_block(bus,msg,3000,&err);
 dbus_message_unref(msg);
 if(dbus_error_is_set(&err)){fprintf(stderr,"%s: %s\n",err.name,err.message);dbus_error_free(&err);}
 return reply;
}
static int simple(DBusConnection *bus,const char *path,const char *iface,const char *method) {
 DBusMessage *reply=send_call(bus,request(path,iface,method));
 if(!reply)return -1;
 dbus_message_unref(reply);return 0;
}
static int property(DBusConnection *bus,const char *path,const char *iface,const char *key) {
 DBusMessage *msg=request(path,"org.freedesktop.DBus.Properties","Get");
 if(!msg)return -1;
 dbus_message_append_args(msg,DBUS_TYPE_STRING,&iface,DBUS_TYPE_STRING,&key,DBUS_TYPE_INVALID);
 DBusMessage *reply=send_call(bus,msg);if(!reply)return -1;
 DBusMessageIter iter,value;int result=-1;
 if(dbus_message_iter_init(reply,&iter)&&dbus_message_iter_get_arg_type(&iter)==DBUS_TYPE_VARIANT){
  dbus_message_iter_recurse(&iter,&value);
  if(dbus_message_iter_get_arg_type(&value)==DBUS_TYPE_BOOLEAN){dbus_bool_t flag;dbus_message_iter_get_basic(&value,&flag);result=flag?1:0;}
 }
 dbus_message_unref(reply);return result;
}
static int set_true(DBusConnection *bus,const char *path,const char *iface,const char *key) {
 DBusMessage *msg=request(path,"org.freedesktop.DBus.Properties","Set");if(!msg)return -1;
 DBusMessageIter iter,value;dbus_bool_t yes=TRUE;
 dbus_message_iter_init_append(msg,&iter);
 dbus_message_iter_append_basic(&iter,DBUS_TYPE_STRING,&iface);
 dbus_message_iter_append_basic(&iter,DBUS_TYPE_STRING,&key);
 dbus_message_iter_open_container(&iter,DBUS_TYPE_VARIANT,"b",&value);
 dbus_message_iter_append_basic(&value,DBUS_TYPE_BOOLEAN,&yes);
 dbus_message_iter_close_container(&iter,&value);
 DBusMessage *reply=send_call(bus,msg);if(!reply)return -1;
 dbus_message_unref(reply);return 0;
}
static DBusHandlerResult agent_message(DBusConnection *bus,DBusMessage *msg,void *data) {
 (void)data;
 if(!dbus_message_has_interface(msg,"org.bluez.Agent1"))return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
 const char *member=dbus_message_get_member(msg),*path=NULL;
 DBusMessage *reply=NULL;
 if(!strcmp(member,"Release")||!strcmp(member,"Cancel"))reply=dbus_message_new_method_return(msg);
 else {
  DBusMessageIter iter;
  if(dbus_message_iter_init(msg,&iter)&&dbus_message_iter_get_arg_type(&iter)==DBUS_TYPE_OBJECT_PATH)
   dbus_message_iter_get_basic(&iter,&path);
  int allowed=path&&!strcmp(path,device);
  if(allowed&&!strcmp(member,"RequestAuthorization"))reply=dbus_message_new_method_return(msg);
  else if(allowed&&!strcmp(member,"AuthorizeService")) {
   const char *uuid=NULL;
   if(dbus_message_iter_next(&iter)&&dbus_message_iter_get_arg_type(&iter)==DBUS_TYPE_STRING)dbus_message_iter_get_basic(&iter,&uuid);
   if(uuid&&(!strcmp(uuid,"0000110b-0000-1000-8000-00805f9b34fb")||!strcmp(uuid,"0000110d-0000-1000-8000-00805f9b34fb")))
    reply=dbus_message_new_method_return(msg);
  }
  /* NoInputNoOutput headphones use Just Works. Do not silently accept
   * passkeys or confirmation requests requiring a user comparison. */
  if(!reply)reply=dbus_message_new_error(msg,"org.bluez.Error.Rejected","Unsupported authentication or different device");
 }
 if(reply){dbus_connection_send(bus,reply,NULL);dbus_message_unref(reply);dbus_connection_flush(bus);}
 return DBUS_HANDLER_RESULT_HANDLED;
}
static int agent_call(DBusConnection *bus,const char *method,int capability) {
 const char *path=AGENT,*cap="NoInputNoOutput";
 DBusMessage *msg=request("/org/bluez","org.bluez.AgentManager1",method);if(!msg)return -1;
 dbus_message_append_args(msg,DBUS_TYPE_OBJECT_PATH,&path,DBUS_TYPE_INVALID);
 if(capability)dbus_message_append_args(msg,DBUS_TYPE_STRING,&cap,DBUS_TYPE_INVALID);
 DBusMessage *reply=send_call(bus,msg);if(!reply)return -1;
 dbus_message_unref(reply);return 0;
}
static long monotime(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec;}
int main(int argc,char **argv) {
 if(argc!=4|| (strcmp(argv[1],"pair")&&strcmp(argv[1],"status"))){fprintf(stderr,"Usage: %s {pair|status} hciN ADDRESS\n",argv[0]);return 2;}
 const char *hci=argv[2],*address=argv[3];
 if(strncmp(hci,"hci",3)||!hci[3])return 2;
 for(const char *p=hci+3;*p;p++)if(!isdigit((unsigned char)*p))return 2;
 if(strlen(address)!=17)return 2;
 char id[18];
 for(int i=0;i<17;i++){
  if(i%3==2){if(address[i]!=':')return 2;id[i]='_';}
  else {if(!isxdigit((unsigned char)address[i]))return 2;id[i]=(char)toupper((unsigned char)address[i]);}
 }id[17]=0;
 char adapter[96];snprintf(adapter,sizeof adapter,"/org/bluez/%s",hci);
 snprintf(device,sizeof device,"%s/dev_%s",adapter,id);
 DBusError err=DBUS_ERROR_INIT;DBusConnection *bus=dbus_bus_get_private(DBUS_BUS_SYSTEM,&err);
 if(!bus){fprintf(stderr,"System D-Bus: %s\n",err.message);dbus_error_free(&err);return 2;}
 dbus_connection_set_exit_on_disconnect(bus,FALSE);
 int result=1,registered=0,discovery=0;
 const char *iface="org.bluez.Device1";
 if(!strcmp(argv[1],"status")){result=property(bus,device,iface,"Paired")==1?0:1;goto done;}
 signal(SIGTERM,stop);signal(SIGINT,stop);
 if(set_true(bus,adapter,"org.bluez.Adapter1","Powered"))goto done;
 if(property(bus,device,iface,"Paired")==1){result=set_true(bus,device,iface,"Trusted")?1:0;goto done;}
 static const DBusObjectPathVTable vtable={.message_function=agent_message};
 if(!dbus_connection_register_object_path(bus,AGENT,&vtable,NULL))goto done;
 if(agent_call(bus,"RegisterAgent",1))goto done;
 registered=1;
 if(agent_call(bus,"RequestDefaultAgent",0))goto done;
 if(simple(bus,adapter,"org.bluez.Adapter1","StartDiscovery"))goto done;
 discovery=1;
 long deadline=monotime()+15;
 while(!stopped&&monotime()<deadline){
  DBusMessage *msg=request(device,"org.freedesktop.DBus.Properties","Get");
  const char *key="Address";dbus_message_append_args(msg,DBUS_TYPE_STRING,&iface,DBUS_TYPE_STRING,&key,DBUS_TYPE_INVALID);
  DBusMessage *reply=send_call(bus,msg);
  if(reply){dbus_message_unref(reply);break;}
  dbus_connection_read_write_dispatch(bus,500);
 }
 if(stopped)goto done;
 DBusMessage *msg=request(device,iface,"Pair");DBusPendingCall *pending=NULL;
 if(!msg||!dbus_connection_send_with_reply(bus,msg,&pending,45000)){if(msg)dbus_message_unref(msg);goto done;}
 dbus_message_unref(msg);dbus_connection_flush(bus);
 deadline=monotime()+46;
 while(!stopped&&monotime()<deadline&&!dbus_pending_call_get_completed(pending))
  if(!dbus_connection_read_write_dispatch(bus,100))break;
 if(!dbus_pending_call_get_completed(pending)){
  dbus_pending_call_cancel(pending);simple(bus,device,iface,"CancelPairing");
 } else {
  DBusMessage *reply=dbus_pending_call_steal_reply(pending);
  if(reply){if(dbus_message_get_type(reply)==DBUS_MESSAGE_TYPE_ERROR)fprintf(stderr,"Pair: %s\n",dbus_message_get_error_name(reply));dbus_message_unref(reply);}
  if(property(bus,device,iface,"Paired")==1&&set_true(bus,device,iface,"Trusted")==0)result=0;
 }
 dbus_pending_call_unref(pending);
done:
 if(discovery)simple(bus,adapter,"org.bluez.Adapter1","StopDiscovery");
 if(registered)agent_call(bus,"UnregisterAgent",0);
 dbus_connection_close(bus);dbus_connection_unref(bus);
 if(!strcmp(argv[1],"pair"))fprintf(stderr,"%s\n",result?"Pairing failed; put configured headphones in pairing mode":"Headphones paired and trusted");
 return result;
}
