
#
# Copyright (c) 2014-2017, Alper Akcan <alper.akcan@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#   # Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#   # Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#   # Neither the name of the <Alper Akcan> nor the
#      names of its contributors may be used to endorse or promote products
#      derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#!/usr//bin/python

import sys
import getopt
import json
import MBusClient as MBusClient

o_destination = None
o_command     = None
o_payload     = None

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

options, remainder = getopt.gnu_getopt(sys.argv[1:], 'd:c:p:f:h', ['help',
                                                                 'destination=',
                                                                 'command=',
                                                                 'payload=',
                                                                 'flood=',
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
                                                                 'mbus-client-ping-interval=',
                                                                 'mbus-client-ping-timeout=',
                                                                 'mbus-client-ping-threshold=',
                                                                 ])

for opt, arg in options:
    if opt in ('-h', '--help'):
        print("command usage:\n" \
              "  -d, --destination              : command destination identifier (default: {})\n" \
              "  -c, --command                  : command identifier (default: {})\n" \
              "  -p, --payload                  : command payload (default: {})\n" \
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
                       o_destination, \
                       o_command, \
                       o_payload, \
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
    elif opt in ('-d', '--destination'):
        o_destination = arg
    elif opt in ('-c', '--command'):
        o_command = arg
    elif opt in ('-p', '--payload'):
        o_payload = json.loads(arg)
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
        self.destination = None
        self.command = None
        self.payload = None
        self.flood = 1
        self.published = 0
        self.finished = 0
        self.status = -1
        self.connected = 0
        self.disconnected = 0
    
def onCommandCallback (client, context, message, status):
    if (message.getResponseStatus() == 0):
        print("{}".format(json.dumps(message.getResponsePayload())))
    context.status = message.getResponseStatus()
    context.finished = 1

def onConnect (client, context, status):
    if (status == MBusClient.MBusClientConnectStatus.Success):
        context.connected = 1
        client.command(context.destination, context.command, context.payload, onCommandCallback, context)
    else:
        if (client.getOptions().connectInterval <= 0):
            context.connected = -1

def onDisconnect (client, context, status):
    if (client.getOptions().connectInterval <= 0):
        context.disconnected = 1

options = MBusClient.MBusClientOptions()

if (mbus_client_identifier != None):
    options.identifier = mbus_client_identifier
if (mbus_client_server_protocol != None):
    options.serverProtocol = mbus_client_server_protocol
if (mbus_client_server_address != None):
    options.serverAddress = mbus_client_server_address
if (mbus_client_server_port != None):
    options.serverPort = int(mbus_client_server_port)
if (mbus_client_connect_timeout != None):
    options.connectTimeout = int(mbus_client_connect_timeout)
if (mbus_client_connect_interval != None):
    options.connectInterval = int(mbus_client_connect_interval)
if (mbus_client_subscribe_timeout != None):
    options.subscribeTimeout = int(mbus_client_subscribe_timeout)
if (mbus_client_register_timeout != None):
    options.registerTimeout = int(mbus_client_register_timeout)
if (mbus_client_command_timeout != None):
    options.commandTimeout = int(mbus_client_command_timeout)
if (mbus_client_publish_timeout != None):
    options.publishTimeout = int(mbus_client_publish_timeout)
if (mbus_client_ping_interval != None):
    options.pingInterval = int(mbus_client_ping_interval)
if (mbus_client_ping_timeout != None):
    options.pingTimeout = int(mbus_client_ping_timeout)
if (mbus_client_ping_threshold != None):
    options.pingThreshold = int(mbus_client_ping_threshold)

if (o_destination == None):
    raise ValueError("destination is invalid")
if (o_command == None):
    raise ValueError("command is invalid")

options.onConnect    = onConnect
options.onDisconnect = onDisconnect
options.onContext    = onParam()
options.onContext.destination = o_destination
options.onContext.command     = o_command
options.onContext.payload     = o_payload

client = MBusClient.MBusClient(options)

client.connect()

while (options.onContext.connected >= 0 and
       options.onContext.disconnected == 0):
    client.run()
    if (options.onContext.finished == 1 and
        client.hasPending() == 0):
        break;

exit(options.onContext.status)
