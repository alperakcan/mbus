#!/usr//bin/python

import sys
import getopt
import json
import MBusClient as MBusClient

mbus_client_identifier        = None
mbus_client_server_protocol   = None
mbus_client_server_address    = None
mbus_client_server_port       = None
mbus_client_connect_timeout   = None
mbus_client_connect_interval  = None
mbus_client_subscribe_timeout = None
mbus_client_register_timeout  = None
mbus_client_command_timeout   = None
mbus_client_publish_timeout   = None
mbus_client_ping_interval     = None
mbus_client_ping_timeout      = None
mbus_client_ping_threshold    = None
subscriptions = []

options, remainder = getopt.gnu_getopt(sys.argv[1:], 's:h', ['help', 
                                                             'subscribe=',
                                                             'mbus-client-identifier=',
                                                             'mbus-client-server-protocol=',
                                                             'mbus-client-server-address=',
                                                             'mbus-client-server-port=',
                                                             'mbus-client-connect-timeout=',
                                                             'mbus-client-connect-interval=',
                                                             'mbus-client-subscribe-timeout=',
                                                             'mbus-client-register-timeout=',
                                                             'mbus-client-command-timeout=',
                                                             'mbus-client-publish-timeout=',
                                                             'mbus-ping-interval=',
                                                             'mbus-ping-timeout=',
                                                             'mbus-ping-threshold=',
                                                             ])

for opt, arg in options:
    if opt in ('-h', '--help'):
        print("subscribe usage:\n" \
              "  --subscribe                    : subscribe to an event identifier\n" \
              "  --mbus-debug-level             : debug level (default: error)\n" \
              "  --mbus-client-identifier       : client identifier (default: {})\n" \
              "  --mbus-client-server-protocol  : server protocol (default: {})\n" \
              "  --mbus-client-server-address   : server address (default: {})\n" \
              "  --mbus-client-server-port      : server port (default: {})\n" \
              "  --mbus-client-connect-timeout  : client connect timeout (default: {})\n" \
              "  --mbus-client-connect-interval : client connect interval (default: {})\n" \
              "  --mbus-client-subscribe-timeout: client subscribe timeout (default: {})\n" \
              "  --mbus-client-register-timeout : client register timeout (default: {})\n" \
              "  --mbus-client-command-timeout  : client command timeout (default: {})\n" \
              "  --mbus-client-publish-timeout  : client publish timeout (default: {})\n" \
              "  --mbus-client-ping-interval    : ping interval (default: {})\n" \
              "  --mbus-client-ping-timeout     : ping timeout (default: {})\n" \
              "  --mbus-client-ping-threshold   : ping threshold (default: {})\n" \
              "  --help                         : this text" \
              .format( \
                       MBusClient.MBusClientDefaults.ClientIdentifier, \
                       MBusClient.MBusClientDefaults.ServerProtocol, \
                       MBusClient.MBusClientDefaults.ServerAddress, \
                       MBusClient.MBusClientDefaults.ServerPort, \
                       MBusClient.MBusClientDefaults.ConnectTimeout, \
                       MBusClient.MBusClientDefaults.ConnectInterval, \
                       MBusClient.MBusClientDefaults.SubscribeTimeout, \
                       MBusClient.MBusClientDefaults.RegisterTimeout, \
                       MBusClient.MBusClientDefaults.CommandTimeout, \
                       MBusClient.MBusClientDefaults.PublishTimeout, \
                       MBusClient.MBusClientDefaults.PingInterval, \
                       MBusClient.MBusClientDefaults.PingTimeout, \
                       MBusClient.MBusClientDefaults.PingThreshold
                    )
              )
        exit(0)
    elif opt in ('-s', '--subscribe'):
        subscriptions.append(arg)
    elif opt == '--mbus-client-identifier':
        mbus_client_identifier = arg
    elif opt == '--mbus-client-server-protocol':
        mbus_client_server_protocol = arg
    elif opt == '--mbus-client-server-address':
        mbus_client_server_address = arg
    elif opt == '--mbus-client-server-port':
        mbus_client_server_port = arg
    elif opt == '--mbus-client-connect-timeout':
        mbus_client_connect_timeout = arg
    elif opt == '--mbus-client-connect-interval':
        mbus_client_connect_interval = arg
    elif opt == '--mbus-client-subscribe-timeout':
        mbus_client_subscribe_timeout = arg
    elif opt == '--mbus-client-register-timeout':
        mbus_client_register_timeout = arg
    elif opt == '--mbus-client-command-timeout':
        mbus_client_command_timeout = arg
    elif opt == '--mbus-client-publish-timeout':
        mbus_client_publish_timeout = arg
    elif opt == '--mbus-client-ping-interval':
        mbus_client_ping_interval = arg
    elif opt == '--mbus-client-ping-timeout':
        mbus_client_ping_timeout = arg
    elif opt == '--mbus-client-ping-threshold':
        mbus_client_ping_threshold = arg

class onParam(object):
    def __init__ (self):
        self.connected = 0
    
def onEventAllAll (self, context, source, event, payload):
    print("{}: {}.{}: {}".format(self.name(), source, event, json.dumps(payload, sort_keys=True, indent=4)));
    
def onEventAllEvent (self, context, source, event, payload):
    print("{}: {}.{}: {}".format(self.name(), source, event, json.dumps(payload, sort_keys=True, indent=4)));
    
def onStatusServerAll (self, context, source, event, payload):
    print("{}: {}.{}: {}".format(self.name(), source, event, json.dumps(payload, sort_keys=True, indent=4)));
    
def onConnect (client, context, status):
    if (status == MBusClient.MBusClientConnectStatus.Success):
        context.connected = 1
        if (len(subscriptions) == 0):
            client.subscribe(MBusClient.MBUS_SERVER_IDENTIFIER, MBusClient.MBUS_METHOD_STATUS_IDENTIFIER_ALL, onStatusServerAll, None)
            client.subscribe(MBusClient.MBUS_METHOD_EVENT_SOURCE_ALL, MBusClient.MBUS_METHOD_EVENT_IDENTIFIER_ALL, onEventAllAll, None)
        else:
            for s in subscriptions:
                client.subscribe(MBusClient.MBUS_METHOD_EVENT_SOURCE_ALL, s, onEventAllEvent, None)
    else:
        context.connected = -1;

def onSubscribed (self, source, event):
    return
    
options = MBusClient.MBusClientOptions()
if (mbus_client_identifier != None):
    options.identifier = mbus_client_identifier
if (mbus_client_server_protocol != None):
    options.serverProtocol = mbus_client_server_protocol
if (mbus_client_server_address != None):
    options.serverAddress = mbus_client_server_address
if (mbus_client_server_port != None):
    options.serverPort = mbus_client_server_port
if (mbus_client_connect_timeout != None):
    options.connectTimeout = mbus_client_connect_timeout
if (mbus_client_connect_interval != None):
    options.connectInterval = mbus_client_connect_interval
if (mbus_client_subscribe_timeout != None):
    options.subscribeTimeout = mbus_client_subscribe_timeout
if (mbus_client_register_timeout != None):
    options.registerTimeout = mbus_client_register_timeout
if (mbus_client_command_timeout != None):
    options.commandTimeout = mbus_client_command_timeout
if (mbus_client_publish_timeout != None):
    options.publishTimeout = mbus_client_publish_timeout
if (mbus_client_ping_interval != None):
    options.pingInterval = mbus_client_ping_interval
if (mbus_client_ping_timeout != None):
    options.pingTimeout = mbus_client_ping_timeout
if (mbus_client_ping_threshold != None):
    options.pingThreshold = mbus_client_ping_threshold
options.onConnect = onConnect
options.onContext = onParam()

client = MBusClient.MBusClient(options)

client.connect()

while (True):
    client.run()
