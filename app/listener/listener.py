
import json
import MBusClient as MBusClient

def onEventAllAll (self, context, source, event, payload):
    print("{}: onEventAllAll {}.{}: {}".format(self.name(), source, event, json.dumps(payload)));
    
def onStatusServerAll (self, context, source, event, payload):
    print("{}: onStatusServerAll {}.{}: {}".format(self.name(), source, event, json.dumps(payload)));
    
def onConnected (self):
    print("{}: onConnected".format(self.name()));
    client.subscribe(MBusClient.MBUS_SERVER_NAME, MBusClient.MBUS_METHOD_STATUS_IDENTIFIER_ALL, onStatusServerAll, None)
    client.subscribe(MBusClient.MBUS_METHOD_EVENT_SOURCE_ALL, MBusClient.MBUS_METHOD_EVENT_IDENTIFIER_ALL, onEventAllAll, None)

def onSubscribed (self, source, event):
    return
    
options = MBusClient.MBusClientOptions()
client = MBusClient.MBusClient(options)
client.onConnected = onConnected
client.onSubscribed = onSubscribed

client.connect()
client.loop()
