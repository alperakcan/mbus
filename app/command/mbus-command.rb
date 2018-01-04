#!/usr/bin/ruby

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

require "getoptlong"
require "json"

require "MBusClient.rb"

o_destination = nil
o_command     = nil
o_payload     = nil

mbus_client_identifier        = nil
mbus_client_server_protocol   = nil
mbus_client_server_address    = nil
mbus_client_server_port       = nil
mbus_client_connect_timeout   = nil
mbus_client_connect_interval  = nil
mbus_client_subscribe_timeout = nil
mbus_client_register_timeout  = nil
mbus_client_command_timeout   = nil
mbus_client_publish_timeout   = nil
mbus_client_ping_interval     = nil
mbus_client_ping_timeout      = nil
mbus_client_ping_threshold    = nil

opts = GetoptLong.new(
  [ "--help"       , "-h", GetoptLong::NO_ARGUMENT ],
    
  [ "--destination", "-d", GetoptLong::REQUIRED_ARGUMENT ],
  [ "--command"    , "-c", GetoptLong::REQUIRED_ARGUMENT ],
  [ "--payload"    , "-p", GetoptLong::REQUIRED_ARGUMENT ],
    
  [ "--mbus-client-identifier"       , GetoptLong::REQUIRED_ARGUMENT ],
  [ "--mbus-client-server-protocol"  , GetoptLong::REQUIRED_ARGUMENT ],
  [ "--mbus-client-server-address"   , GetoptLong::REQUIRED_ARGUMENT ],
  [ "--mbus-client-server-port"      , GetoptLong::REQUIRED_ARGUMENT ],
  [ "--mbus-client-connect-timeout"  , GetoptLong::REQUIRED_ARGUMENT ],
  [ "--mbus-client-connect-interval" , GetoptLong::REQUIRED_ARGUMENT ],
  [ "--mbus-client-subscribe-timeout", GetoptLong::REQUIRED_ARGUMENT ],
  [ "--mbus-client-register-timeout" , GetoptLong::REQUIRED_ARGUMENT ],
  [ "--mbus-client-command-timeout"  , GetoptLong::REQUIRED_ARGUMENT ],
  [ "--mbus-client-publish-timeout"  , GetoptLong::REQUIRED_ARGUMENT ],
  [ "--mbus-client-ping-interval"    , GetoptLong::REQUIRED_ARGUMENT ],
  [ "--mbus-client-ping-timeout"     , GetoptLong::REQUIRED_ARGUMENT ],
  [ "--mbus-client-ping-threshold"   , GetoptLong::REQUIRED_ARGUMENT ]
)

opts.each do |opt, arg|
  case opt
    when "--help"
    puts "command usage:\n" \
         "  -d, --destination              : command destination identifier (default: %s)\n" \
         "  -c, --command                  : command identifier (default: %s)\n" \
         "  -p, --payload                  : command payload (default: %s)\n" \
         "  --mbus-debug-level             : debug level (default: error)\n" \
         "  --mbus-client-identifier       : client identifier (default: %s)\n" \
         "  --mbus-client-server-protocol  : server protocol (default: %s)\n" \
         "  --mbus-client-server-address   : server address (default: %s)\n" \
         "  --mbus-client-server-port      : server port (default: %s)\n" \
         "  --mbus-client-connect-timeout  : client connect timeout (default: %s)\n" \
         "  --mbus-client-connect-interval : client connect interval (default: %s)\n" \
         "  --mbus-client-subscribe-timeout: client subscribe timeout (default: %s)\n" \
         "  --mbus-client-register-timeout : client register timeout (default: %s)\n" \
         "  --mbus-client-command-timeout  : client command timeout (default: %s)\n" \
         "  --mbus-client-publish-timeout  : client publish timeout (default: %s)\n" \
         "  --mbus-client-ping-interval    : ping interval (default: %s)\n" \
         "  --mbus-client-ping-timeout     : ping timeout (default: %s)\n" \
         "  --mbus-client-ping-threshold   : ping threshold (default: %s)\n" \
         "  --help                         : this text" \
         % [
           o_destination,
           o_command,
           o_payload,
           MBusClient::MBusClientDefaults::IDENTIFIER,
           MBusClient::MBusClientDefaults::SERVER_PROTOCOL,
           MBusClient::MBusClientDefaults::SERVER_ADDRESS,
           MBusClient::MBusClientDefaults::SERVER_PORT,
           MBusClient::MBusClientDefaults::CONNECT_TIMEOUT,
           MBusClient::MBusClientDefaults::CONNECT_INTERVAL,
           MBusClient::MBusClientDefaults::SUBSCRIBE_TIMEOUT,
           MBusClient::MBusClientDefaults::REGISTER_TIMEOUT,
           MBusClient::MBusClientDefaults::COMMAND_TIMEOUT,
           MBusClient::MBusClientDefaults::PUBLISH_TIMEOUT,
           MBusClient::MBusClientDefaults::PING_INTERVAL,
           MBusClient::MBusClientDefaults::PING_TIMEOUT,
           MBusClient::MBusClientDefaults::PING_THRESHOLD
         ]
         exit(0)
    when "--destination"
      o_destination = arg
    when "--command"
      o_command     = arg
    when "--payload"
      o_payload     = JSON.parse(arg)
    when "--mbus-client-identifier"
      mbus_client_identifier        = arg
    when "--mbus-client-server-protocol"
      mbus_client_server_protocol   = arg
    when "--mbus-client-server-address"
      mbus_client_server_address    = arg
    when "--mbus-client-server-port"
      mbus_client_server_port       = arg
    when "--mbus-client-connect-timeout"
      mbus_client_connect_timeout   = arg
    when "--mbus-client-connect-interval"
      mbus_client_connect_interval  = arg
    when "--mbus-client-subscribe-timeout"
      mbus_client_subscribe_timeout = arg
    when "--mbus-client-register-timeout"
      mbus_client_register_timeout  = arg
    when "--mbus-client-command-timeout"
      mbus_client_command_timeout   = arg
    when "--mbus-client-publish-timeout"
      mbus_client_publish_timeout   = arg
    when "--mbus-client-ping-interval"
      mbus_client_ping_interval     = arg
    when "--mbus-client-ping-timeout"
      mbus_client_ping_timeout      = arg
    when "--mbus-client-ping-threshold"
      mbus_client_ping_threshold    = arg
  end
end

class CallbackParam
  attr_accessor :destination
  attr_accessor :command
  attr_accessor :payload
  attr_accessor :finished
  attr_accessor :status
  attr_accessor :connected
  attr_accessor :disconnected

  def initialize
    @destination = nil
    @command = nil
    @payload = nil
    @finished = 0
    @status = -1
    @connected = 0
    @disconnected = 0
  end
end

def onCommandCallback (client, context, message, status)
  if (message.getResponseStatus() == 0)
    if (message.getResponsePayload() != nil)
      puts "%s" % [ message.getResponsePayload() ]
     end
  end
  context.status = message.getResponseStatus()
  context.finished = 1
end

def onConnect (client, context, status)
  if (status == MBusClient::MBusClientConnectStatus::SUCCESS)
    context.connected = 1
    client.command(context.destination, context.command, context.payload, method(:onCommandCallback), context)
  else
    if (client.getOptions().connectInterval <= 0)
      context.connected = -1
    end
  end
end

def onDisconnect (client, context, status)
  if (client.getOptions().connectInterval <= 0)
    context.disconnected = 1
  end
end

options = MBusClient::MBusClientOptions.new()
if (mbus_client_identifier != nil)
    options.identifier = mbus_client_identifier
end
if (mbus_client_server_protocol != nil)
    options.serverProtocol = mbus_client_server_protocol
end
if (mbus_client_server_address != nil)
    options.serverAddress = mbus_client_server_address
end
if (mbus_client_server_port != nil)
    options.serverPort = mbus_client_server_port.to_i()
end
if (mbus_client_connect_timeout != nil)
    options.connectTimeout = mbus_client_connect_timeout.to_i()
end
if (mbus_client_connect_interval != nil)
    options.connectInterval = mbus_client_connect_interval.to_i()
end
if (mbus_client_subscribe_timeout != nil)
    options.subscribeTimeout = mbus_client_subscribe_timeout.to_i()
end
if (mbus_client_register_timeout != nil)
    options.registerTimeout = mbus_client_register_timeout.to_i()
end
if (mbus_client_command_timeout != nil)
    options.commandTimeout = mbus_client_command_timeout.to_i()
end
if (mbus_client_publish_timeout != nil)
    options.publishTimeout = mbus_client_publish_timeout.to_i()
end
if (mbus_client_ping_interval != nil)
    options.pingInterval = mbus_client_ping_interval.to_i()
end
if (mbus_client_ping_timeout != nil)
    options.pingTimeout = mbus_client_ping_timeout.to_i()
end
if (mbus_client_ping_threshold != nil)
    options.pingThreshold = mbus_client_ping_threshold.to_i()
end


options.onConnect = method(:onConnect)
options.onDisconnect = method(:onDisconnect)

options.onContext = CallbackParam.new()
options.onContext.destination = o_destination
options.onContext.command     = o_command
options.onContext.payload     = o_payload

client = MBusClient::MBusClient.new(options)
client.connect()

while (options.onContext.connected >= 0 and
       options.onContext.disconnected == 0)
  client.run()
  if (options.onContext.finished == 1 and
    client.hasPending() == 0)
    break;
  end
end

exit(options.onContext.status)
