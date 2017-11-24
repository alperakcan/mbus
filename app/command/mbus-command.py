#!/usr//bin/python

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

g_destination = MBusClient.MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS
g_command = None
g_payload = {}

g_finished = 0

options, remainder = getopt.gnu_getopt(sys.argv[1:], 'd:c:p:h', ['help', 
                                                             'destination=',
                                                             'command=',
                                                             'payload=',
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
              "  --destination          : destination identifier\n" \
              "  --command              : command identifier\n" \
              "  --payload              : payload json\n" \
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
    elif opt in ('-d', '--destination'):
        g_destination = arg
    elif opt in ('-c', '--command'):
        g_command = arg
    elif opt in ('-p', '--payload'):
        g_payload = json.loads(arg)
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

if g_destination == None:
    print("destination is invalid");
    exit(-1)
    
if g_command == None:
    print("command is invalid");
    exit(-1)
    
if g_payload == None:
    print("payload is invalid");
    exit(-1)

def onCommandResult (self, context, source, command, result, payload):
    global g_finished
    if (result != 0):
        print("{}".format(result))
    print("{}".format(json.dumps(payload, sort_keys=True, indent=4)))
    g_finished = 1
    
def onStatusConnected (self, context, source, event, payload):
    client.command(g_destination, g_command, g_payload, onCommandResult, None)
    
def onConnected (self):
    client.subscribe(MBusClient.MBUS_SERVER_IDENTIFIER, MBusClient.MBUS_SERVER_STATUS_CONNECTED, onStatusConnected, None)

options = MBusClient.MBusClientOptions()
client = MBusClient.MBusClient(options)
client.onConnected = onConnected

client.connect()
while (True):
    client.run(MBusClient.MBUS_CLIENT_RUN_TIMEOUT)
    if (g_finished == 1 and \
        client.pending() == False):
        break
