
import sys
import getopt
import json
import MBusClient as MBusClient

mbus_server_protocol = None
mbus_server_address = None
mbus_server_port = None
mbus_client_name = None
mbus_ping_interval = None
mbus_ping_timeout = None
mbus_ping_threshold = None
subscriptions = []

options, remainder = getopt.gnu_getopt(sys.argv[1:], 's:h', ['help', 
                                                             'subscribe=',
                                                             'mbus-server-protocol=',
                                                             'mbus-server-address=',
                                                             'mbus-server-port=',
                                                             'mbus-client-name=',
                                                             'mbus-ping-interval=',
                                                             'mbus-ping-timeout=',
                                                             'mbus-ping-threshold=',
                                                             ])

for opt, arg in options:
    if opt in ('-h', '--help'):
        print("listener usage:\n" \
              "  --subscribe             : subscribe to an event identifier\n" \
              "  --mbus-debug-level     : debug level (default: error)\n" \
              "  --mbus-server-protocol : server protocol (default: uds)\n" \
              "  --mbus-server-address  : server address (default: /tmp/mbus-server-uds)\n" \
              "  --mbus-server-port     : server port (default: 0)\n" \
              "  --mbus-client-name     : client name (overrides api parameter)\n" \
              "  --mbus-ping-interval   : ping interval (overrides api parameter) (default: {})\n" \
              "  --mbus-ping-timeout    : ping timeout (overrides api parameter) (default: {})\n" \
              "  --mbus-ping-threshold  : ping threshold (overrides api parameter) (default: {})\n" \
              "  --help                 : this text" \
              .format( \
                       MBusClient.MBUS_CLIENT_OPTIONS_DEFAULT_PING_INTERVAL, \
                       MBusClient.MBUS_CLIENT_OPTIONS_DEFAULT_PING_TIMEOUT, \
                       MBusClient.MBUS_CLIENT_OPTIONS_DEFAULT_PING_THRESHOLD
                    )
              )
        exit(0)
    elif opt in ('-s', '--subscribe'):
        subscriptions.append(arg)
    elif opt == '--mbus-server-protocol':
        mbus_server_protocol = arg
    elif opt == '--mbus-server-address':
        mbus_server_address = arg
    elif opt == '--mbus-server-port':
        mbus_server_port = arg
    elif opt == '--mbus-client-name':
        mbus_client_name = arg
    elif opt == '--mbus-ping-interval':
        mbus_ping_interval = arg
    elif opt == '--mbus-ping-timeout':
        mbus_ping_timeout = arg
    elif opt == '--mbus-ping-threshold':
        mbus_ping_threshold = arg

def onEventAllAll (self, context, source, event, payload):
    print("{}: {}.{}: {}".format(self.name(), source, event, json.dumps(payload)));
    
def onEventAllEvent (self, context, source, event, payload):
    print("{}: {}.{}: {}".format(self.name(), source, event, json.dumps(payload)));
    
def onStatusServerAll (self, context, source, event, payload):
    print("{}: {}.{}: {}".format(self.name(), source, event, json.dumps(payload)));
    
def onConnected (self):
    print("{}: onConnected".format(self.name()));
    if (len(subscriptions) == 0):
        client.subscribe(MBusClient.MBUS_SERVER_NAME, MBusClient.MBUS_METHOD_STATUS_IDENTIFIER_ALL, onStatusServerAll, None)
        client.subscribe(MBusClient.MBUS_METHOD_EVENT_SOURCE_ALL, MBusClient.MBUS_METHOD_EVENT_IDENTIFIER_ALL, onEventAllAll, None)
    else:
        for s in subscriptions:
            client.subscribe(MBusClient.MBUS_METHOD_EVENT_SOURCE_ALL, s, onEventAllEvent, None)

def onSubscribed (self, source, event):
    return
    
options = MBusClient.MBusClientOptions()
client = MBusClient.MBusClient(options)
client.onConnected = onConnected
client.onSubscribed = onSubscribed

client.connect()
client.loop()
