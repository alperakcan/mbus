
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

require "json"
require "socket"
include Socket::Constants

module MBusClient
  MBUS_METHOD_TYPE_COMMAND                    = "org.mbus.method.type.command"
  MBUS_METHOD_TYPE_EVENT                      = "org.mbus.method.type.event"
  MBUS_METHOD_TYPE_RESULT                     = "org.mbus.method.type.result"
  
  MBUS_METHOD_SEQUENCE_START                  = 1
  MBUS_METHOD_SEQUENCE_END                    = 9999
  
  MBUS_METHOD_EVENT_SOURCE_ALL                = "org.mbus.method.event.source.all"
  MBUS_METHOD_EVENT_DESTINATION_ALL           = "org.mbus.method.event.destination.all"
  MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS   = "org.mbus.method.event.destination.subscribers"
  MBUS_METHOD_EVENT_IDENTIFIER_ALL            = "org.mbus.method.event.identifier.all"
  
  MBUS_METHOD_TAG_TYPE                        = "org.mbus.method.tag.type"
  MBUS_METHOD_TAG_SOURCE                      = "org.mbus.method.tag.source"
  MBUS_METHOD_TAG_DESTINATION                 = "org.mbus.method.tag.destination"
  MBUS_METHOD_TAG_IDENTIFIER                  = "org.mbus.method.tag.identifier"
  MBUS_METHOD_TAG_SEQUENCE                    = "org.mbus.method.tag.sequence"
  MBUS_METHOD_TAG_TIMEOUT                     = "org.mbus.method.tag.timeout"
  MBUS_METHOD_TAG_PAYLOAD                     = "org.mbus.method.tag.payload"
  MBUS_METHOD_TAG_STATUS                      = "org.mbus.method.tag.status"
  
  MBUS_SERVER_IDENTIFIER                      = "org.mbus.server"
  
  MBUS_SERVER_COMMAND_CREATE                  = "command.create"
  MBUS_SERVER_COMMAND_EVENT                   = "command.event"
  MBUS_SERVER_COMMAND_CALL                    = "command.call"
  MBUS_SERVER_COMMAND_RESULT                  = "command.result"
  MBUS_SERVER_COMMAND_STATUS                  = "command.status"
  MBUS_SERVER_COMMAND_CLIENTS                 = "command.clients"
  MBUS_SERVER_COMMAND_SUBSCRIBE               = "command.subscribe"
  MBUS_SERVER_COMMAND_UNSUBSCRIBE             = "command.unsubscribe"
  MBUS_SERVER_COMMAND_REGISTER                = "command.register"
  MBUS_SERVER_COMMAND_UNREGISTER              = "command.unregister"
  MBUS_SERVER_COMMAND_CLOSE                   = "command.close"
  
  MBUS_SERVER_EVENT_CONNECTED                 = "org.mbus.server.event.connected"
  MBUS_SERVER_EVENT_DISCONNECTED              = "org.mbus.server.event.disconnected"
  MBUS_SERVER_EVENT_SUBSCRIBED                = "org.mbus.server.event.subscribed"
  MBUS_SERVER_EVENT_UNSUBSCRIBED              = "org.mbus.server.event.unsubscribed"
  
  MBUS_SERVER_EVENT_PING                      = "org.mbus.server.event.ping"
  MBUS_SERVER_EVENT_PONG                      = "org.mbus.server.event.pong"

  class MBusClientClock
    
    def self.get
      return Process.clock_gettime(Process::CLOCK_MONOTONIC_RAW, :millisecond)
    end
    
    def self.after (a, b)
      if (((((b) - (a)) < 0)))
        return true
       end
       return false
    end
    
    def self.before (a, b)
      return after(b, a)
    end
    
  end
  
  class MBusClientDefaults
    
    IDENTIFIER          = nil
    
    SERVER_TCP_PROTOCOL = "tcp"
    SERVER_TCP_ADDRESS  = "127.0.0.1"
    SERVER_TCP_PORT     = 8000
    
    SERVER_PROTOCOL     = SERVER_TCP_PROTOCOL
    SERVER_ADDRESS      = SERVER_TCP_ADDRESS
    SERVER_PORT         = SERVER_TCP_PORT
    
    RUN_TIMEOUT         = 1000
    
    CONNECT_TIMEOUT     = 30000
    CONNECT_INTERVAL    = 0
    SUBSCRIBE_TIMEOUT   = 30000
    REGISTER_TIMEOUT    = 30000
    COMMAND_TIMEOUT     = 30000
    PUBLISH_TIMEOUT     = 30000
    
    PING_INTERVAL       = 180000
    PING_TIMEOUT        = 5000
    PING_THRESHOLD      = 2
    
  end
  
  class MBusClientQoS
    
    AT_MOST_ONCE  = 0
    AT_LEAST_ONCE = 1
    EXACTLY_ONCE  = 2
    
  end
  
  class MBusClientState
    
    UNKNOWN       = 0
    CONNECTING    = 1
    CONNECTED     = 2
    DISCONNECTING = 3
    DISCONNECTED  = 4
    
  end
  
  class MBusClientConnectStatus
    
    SUCCESS                   = 0
    INTERNAL_ERROR            = 1
    INVALID_PROTOCOL          = 2
    CONNECTION_REFUSED        = 3
    SERVER_UNAVAILABLE        = 4
    TIMEOUT                   = 5
    CANCELED                  = 6
    INVALID_PROTOCOL_VERSION  = 7
    INVALID_IDENTIFIER        = 8
    SERVER_ERROR              = 9
    
    def self.string (status)
      if (status == SUCCESS)
        return "success"
      elsif (status == INTERNAL_ERROR)
        return "internal error"
      elsif (status == INVALID_PROTOCOL)
        return "invalid protocol"
      elsif (status == CONNECTION_REFUSED)
        return "connection refused"
      elsif (status == SERVER_UNAVAILABLE)
        return "server unavailable"
      elsif (status == TIMEOUT)
        return "timeout"
      elsif (status == CANCELED)
        return "canceled"
      elsif (status == INVALID_PROTOCOL_VERSION)
        return "invalid protocol version"
      elsif (status == INVALID_IDENTIFIER)
        return "invalid identifier"
      elsif (status == SERVER_ERROR)
        return "server error"
      else
        return "unknown"
      end
    end
    
  end
  
  class MBusClientDisconnectStatus
    
    SUCCESS           = 0
    INTERNAL_ERROR    = 1
    CONNECTION_CLOSED = 2
    CANCELED          = 3
    PING_TIMEOUT      = 4

    def self.string (status)
      if (status == SUCCESS)
        return "success"
      elsif (status == INTERNAL_ERROR)
        return "internal error"
      elsif (status == CONNECTION_CLOSED)
        return "connection closed"
      elsif (status == CANCELED)
        return "canceled"
      elsif (status == PING_TIMEOUT)
        return "ping timeout"
      else
        return "unknown"
      end
    end
    
  end

  class MBusClientPublishStatus
    
    SUCCESS           = 0
    INTERNAL_ERROR    = 1
    TIMEOUT           = 2
    CANCELED          = 3

    def self.string (status)
      if (status == SUCCESS)
        return "success"
      elsif (status == INTERNAL_ERROR)
        return "internal error"
      elsif (status == TIMEOUT)
        return "timeout"
      elsif (status == CANCELED)
        return "canceled"
      else
        return "unknown"
      end
    end
    
  end

  class MBusClientSubscribeStatus
    
    SUCCESS           = 0
    INTERNAL_ERROR    = 1
    TIMEOUT           = 2
    CANCELED          = 3

    def self.string (status)
      if (status == SUCCESS)
        return "success"
      elsif (status == INTERNAL_ERROR)
        return "internal error"
      elsif (status == TIMEOUT)
        return "timeout"
      elsif (status == CANCELED)
        return "canceled"
      else
        return "unknown"
      end
    end
    
  end

  class MBusClientUnsubscribeStatus
    
    SUCCESS           = 0
    INTERNAL_ERROR    = 1
    TIMEOUT           = 2
    CANCELED          = 3

    def self.string (status)
      if (status == SUCCESS)
        return "success"
      elsif (status == INTERNAL_ERROR)
        return "internal error"
      elsif (status == TIMEOUT)
        return "timeout"
      elsif (status == CANCELED)
        return "canceled"
      else
        return "unknown"
      end
    end
    
  end

  class MBusClientRegisterStatus
    
    SUCCESS           = 0
    INTERNAL_ERROR    = 1
    TIMEOUT           = 2
    CANCELED          = 3

    def self.string (status)
      if (status == SUCCESS)
        return "success"
      elsif (status == INTERNAL_ERROR)
        return "internal error"
      elsif (status == TIMEOUT)
        return "timeout"
      elsif (status == CANCELED)
        return "canceled"
      else
        return "unknown"
      end
    end
    
  end

  class MBusClientUnregisterStatus
    
    SUCCESS           = 0
    INTERNAL_ERROR    = 1
    TIMEOUT           = 2
    CANCELED          = 3

    def self.string (status)
      if (status == SUCCESS)
        return "success"
      elsif (status == INTERNAL_ERROR)
        return "internal error"
      elsif (status == TIMEOUT)
        return "timeout"
      elsif (status == CANCELED)
        return "canceled"
      else
        return "unknown"
      end
    end
    
  end

  class MBusClientCommandStatus
    
    SUCCESS           = 0
    INTERNAL_ERROR    = 1
    TIMEOUT           = 2
    CANCELED          = 3

    def self.string (status)
      if (status == SUCCESS)
        return "success"
      elsif (status == INTERNAL_ERROR)
        return "internal error"
      elsif (status == TIMEOUT)
        return "timeout"
      elsif (status == CANCELED)
        return "canceled"
      else
        return "unknown"
      end
    end
    
  end
  
  class MBusClientWakeUpReason
    
    BREAK      = 0
    CONNECT    = 1
    DISCONNECT = 2
    
  end
  
  class MBusClientOptions
    
    attr_accessor :identifier
    
    attr_accessor :serverProtocol
    attr_accessor :serverAddress
    attr_accessor :serverPort
    
    attr_accessor :connectTimeout
    attr_accessor :connectInterval
    attr_accessor :subscribeTimeout
    attr_accessor :registerTimeout
    attr_accessor :commandTimeout
    attr_accessor :publishTimeout
    
    attr_accessor :pingInterval
    attr_accessor :pingTimeout
    attr_accessor :pingThreshold
    
    attr_accessor :onConnect
    attr_accessor :onDisconnect
    attr_accessor :onMessage
    attr_accessor :onResult
    attr_accessor :onRoutine
    attr_accessor :onPublish
    attr_accessor :onSubscribe
    attr_accessor :onUnsubscribe
    attr_accessor :onRegistered
    attr_accessor :onUnregistered
    attr_accessor :onContext
    
    def initialize
      @identifier       = nil
      
      @serverProtocol   = nil
      @serverAddress    = nil
      @serverPort       = nil
      
      @connectTimeout   = nil
      @connectInterval  = nil
      @subscribeTimeout = nil
      @registerTimeout  = nil
      @commandTimeout   = nil
      @publishTimeout   = nil
      
      @pingInterval     = nil
      @pingTimeout      = nil
      @pingThreshold    = nil
      
      @onConnect        = nil
      @onDisconnect     = nil
      @onMessage        = nil
      @onResult         = nil
      @onRoutine        = nil
      @onPublish        = nil
      @onSubscribe      = nil
      @onUnsubscribe    = nil
      @onRegistered     = nil
      @onUnregistered   = nil
      @onContext        = nil
    end
    
  end
  
  class MBusClientSubscription
      
    attr_reader :source
    attr_reader :identifier
    attr_reader :callback
    attr_reader :context

    def initialize (source, identifier, callback, context)
       @source     = source
       @identifier = identifier
       @callback   = callback
       @context    = context
     end
      
  end
  
  class MBusClientRequest
    
    attr_reader :type
    attr_reader :destination
    attr_reader :identifier
    attr_reader :sequence
    attr_reader :payload
    attr_reader :callback
    attr_reader :context
    attr_reader :timeout
    attr_reader :createdAt
    
    def initialize (type, destination, identifier, sequence, payload, callback, context, timeout)
      @type        = type
      @destination = destination
      @identifier  = identifier
      @sequence    = sequence
      @payload     = payload
      @callback    = callback
      @context     = context
      @timeout     = timeout
      @createdAt   = MBusClientClock::get()
    end
    
    def stringify
      request = {}
      request[MBUS_METHOD_TAG_TYPE]        = @type
      request[MBUS_METHOD_TAG_DESTINATION] = @destination
      request[MBUS_METHOD_TAG_IDENTIFIER]  = @identifier
      request[MBUS_METHOD_TAG_SEQUENCE]    = @sequence
      request[MBUS_METHOD_TAG_PAYLOAD]     = @payload
      request[MBUS_METHOD_TAG_TIMEOUT]     = @timeout
      return JSON.dump(request)
    end
    
  end

  class MBusClientMessageEvent
    
    def initialize (request)
      @request = request
    end
    
    def getSource
      return @request[MBUS_METHOD_TAG_SOURCE]
    end

    def getDestination
      return @request[MBUS_METHOD_TAG_DESTINATION]
    end

    def getIdentifier
      return @request[MBUS_METHOD_TAG_IDENTIFIER]
    end
    
    def getPayload
      return @request[MBUS_METHOD_TAG_PAYLOAD]
    end
  end

  class MBusClientMessageCommand

    def initialize (request, response)
      @request = JSON.parse(request.stringify())
      @response = response
    end
    
    def getRequestDestination
      return @request[MBUS_METHOD_TAG_DESTINATION]
    end
    
    def getRequestIdentifier
      return @request[MBUS_METHOD_TAG_IDENTIFIER]
    end
    
    def getRequestPayload
      return @request[MBUS_METHOD_TAG_PAYLOAD]
    end
    
    def getResponseStatus
      return @response[MBUS_METHOD_TAG_STATUS]
    end

    def getResponsePayload
      return @response[MBUS_METHOD_TAG_PAYLOAD]
    end
    
  end

  class MBusClient

    private
        
    attr_accessor :options
    attr_accessor :state
    attr_accessor :socket
    attr_accessor :requests
    attr_accessor :pendings
    attr_accessor :routines
    attr_accessor :subscriptions
    attr_accessor :incoming
    attr_accessor :outgoing
    attr_writer   :identifier
    attr_accessor :connectTsms
    attr_accessor :pingInterval
    attr_accessor :pingTimeout
    attr_accessor :pingThreshold
    attr_accessor :pingSendTsms
    attr_accessor :pongRecvTsms
    attr_accessor :pingWaitPong
    attr_accessor :pongMissedCount
    attr_accessor :compression
    attr_accessor :socketConnected
    attr_accessor :sequence
    attr_accessor :wakeupRead
    attr_accessor :wakeupWrite
    
    def notifyPublish (request, status)
      if (@options.onPublish != nil)
        message = MBusClientMessageEvent.new(request)
        @options.onPublish.call(self, @options.onContext, message, status)
      end
    end

    def notifySubscribe (source, event, status)
      if (@options.onSubscribe != nil)
        @options.onSubscribe.call(self, @options.onContext, source, event, status)
      end
    end

    def notifyUnsubscribe (source, event, status)
      if (@options.onUnsubscribe != nil)
        @options.onUnsubscribe.call(self, @options.onContext, source, event, status)
      end
    end

    def notifyRegistered (command, status)
      if (@options.onRegistered != nil)
        @options.onRegistered.call(self, @options.onContext, command, status)
      end
    end

    def notifyUnregistered (command, status)
      if (@options.onUnregistered != nil)
        @options.onUnregistered.call(self, @options.onContext, command, status)
      end
    end

    def notifyCommand (request, response, status)
      callback = @options.onResult
      context = @options.onContext
      if (request.callback != nil)
        callback = request.callback
        context = request.context
      end
      if (callback != nil)
        message = MBusClientMessageCommand.new(request, response)
        callback.call(self, context, message, status)
      end
    end
    
    def notifyConnect (status)
      if (@options.onConnect != nil)
        @options.onConnect.call(self, @options.onContext, status)
      end
    end

    def notifyDisonnect (status)
      if (@options.onDisconnect != nil)
        @options.onDisconnect.call(self, @options.onContext, status)
      end
    end

    def reset
      if (@socket != nil)
        @socket.close()
      end

      @requests.each do |request|
        if (request.type == MBUS_METHOD_TYPE_EVENT)
            if (request.destination != MBUS_SERVER_IDENTIFIER and
                request.identifier != MBUS_SERVER_EVENT_PING)
              notifyPublish(json.loads(request.stringify()), MBusClientPublishStatus::CANCELED)
            end
        elsif (request.type == MBUS_METHOD_TYPE_COMMAND)
            if (request.identifier == MBUS_SERVER_COMMAND_EVENT)
              notifyPublish(request.payload, MBusClientPublishStatus::CANCELED)
            elsif (request.identifier == MBUS_SERVER_COMMAND_SUBSCRIBE)
              notifySubscribe(request.payload["source"], request.payload["event"], MBusClientSubscribeStatus::CANCELED)
            elsif (request.identifier == MBUS_SERVER_COMMAND_UNSUBSCRIBE)
              notifyUnsubscribe(request.payload["source"], request.payload["event"], MBusClientUnsubscribeStatus::CANCELED)
            elsif (request.identifier == MBUS_SERVER_COMMAND_REGISTER)
              notifyRegistered(request.payload["command"], MBusClientRegisterStatus::CANCELED)
            elsif (request.identifier == MBUS_SERVER_COMMAND_UNREGISTER)
              notifyUnregistered(request.payload["command"], MBusClientUnregisterStatus::CANCELED)
            else
              notifyCommand(request, nil, MBusClientCommandStatus::CANCELED)
            end
        end
      end
      
      @pendings.each do |request|
        if (request.type == MBUS_METHOD_TYPE_EVENT)
            if (request.destination != MBUS_SERVER_IDENTIFIER and
                request.identifier != MBUS_SERVER_EVENT_PING)
              notifyPublish(json.loads(request.stringify()), MBusClientPublishStatus::CANCELED)
            end
        elsif (request.type == MBUS_METHOD_TYPE_COMMAND)
            if (request.identifier == MBUS_SERVER_COMMAND_EVENT)
              notifyPublish(request.payload, MBusClientPublishStatus::CANCELED)
            elsif (request.identifier == MBUS_SERVER_COMMAND_SUBSCRIBE)
              notifySubscribe(request.payload["source"], request.payload["event"], MBusClientSubscribeStatus::CANCELED)
            elsif (request.identifier == MBUS_SERVER_COMMAND_UNSUBSCRIBE)
              notifyUnsubscribe(request.payload["source"], request.payload["event"], MBusClientUnsubscribeStatus::CANCELED)
            elsif (request.identifier == MBUS_SERVER_COMMAND_REGISTER)
              notifyRegistered(request.payload["command"], MBusClientRegisterStatus::CANCELED)
            elsif (request.identifier == MBUS_SERVER_COMMAND_UNREGISTER)
              notifyUnregistered(request.payload["command"], MBusClientUnregisterStatus::CANCELED)
            else
              notifyCommand(request, nil, MBusClientCommandStatus::CANCELED)
            end
        end
      end
      
      @routines.each do |routine|
        notifyUnregistered(routine.identifier, MBusClientUnregisterStatus::CANCELED)
      end
      
      @subscriptions.each do |subscription|
        notifyUnsubscribe(subscription.source, subscription.identifier, MBusClientUnsubscribeStatus::CANCELED)
      end
      
      @identifier      = nil
      @socket          = nil
      @pingInterval    = 0
      @pingTimeout     = 0
      @pingThreshold   = 0
      @pingSendTsms    = 0
      @pongRecvTsms    = 0
      @pingWaitPong    = 0
      @pongMissedCount = 0
      @sequence        = MBUS_METHOD_SEQUENCE_START
      @compression     = nil
      @socketConnected = 0
      
      @incoming.clear()
      @outgoing.clear()

      @requests.clear()
      @pendings.clear()
      @routines.clear()
      @subscriptions.clear()
    end
    
    def commandSubscribeResponse (this, context, message, status)
      subscription = context
      if (status != MBusClientCommandStatus::SUCCESS)
        if (status == MBusClientCommandStatus::INTERNAL_ERROR)
          cstatus = MBusClientSubscribeStatus::INTERNAL_ERROR
        elsif (status == MBusClientCommandStatus::TIMEOUT)
          cstatus = MBusClientSubscribeStatus::TIMEOUT
        else
          cstatus = MBusClientSubscribeStatus::INTERNAL_ERROR
        end
      elsif (message.getResponseStatus() == 0)
        cstatus = MBusClientSubscribeStatus::SUCCESS
        if (subscription != nil)
          @subscriptions.push(subscription)
        end
      else
        cstatus = MBusClientSubscribeStatus::INTERNAL_ERROR
      end
      notifySubscribe(message.getRequestPayload()["source"], message.getRequestPayload()["event"], cstatus)
    end
    
    def commandUnsubscribeResponse (this, context, message, status)
      subscription = context
      if (status != MBusClientCommandStatus::SUCCESS)
        if (status == MBusClientCommandStatus::INTERNAL_ERROR)
          cstatus = MBusClientUnsubscribeStatus::INTERNAL_ERROR
        elsif (status == MBusClientCommandStatus::TIMEOUT)
          cstatus = MBusClientUnsubscribeStatus::TIMEOUT
        else
          cstatus = MBusClientUnsubscribeStatus::INTERNAL_ERROR
        end
      elsif (message.getResponseStatus() == 0)
        cstatus = MBusClientUnsubscribeStatus::SUCCESS
        if (subscription != nil)
          @subscriptions.remove(subscription)
        end
      else
        cstatus = MBusClientUnsubscribeStatus::INTERNAL_ERROR
      end
      notifyUnsubscribe(message.getRequestPayload()["source"], message.getRequestPayload()["event"], cstatus)
    end
    
    def commandEventResponse (this, context, message, status)
      if (status != MBusClientCommandStatus::SUCCESS)
        if (status == MBusClientCommandStatus::INTERNAL_ERROR)
          cstatus = MBusClientPublishStatus::INTERNAL_ERROR
        elsif (status == MBusClientCommandStatus::TIMEOUT)
          cstatus = MBusClientPublishStatus::TIMEOUT
        else
          cstatus = MBusClientPublishStatus::INTERNAL_ERROR
        end
      elsif (message.getResponseStatus() == 0)
        cstatus = MBusClientPublishStatus::SUCCESS
      else
        cstatus = MBusClientPublishStatus::INTERNAL_ERROR
      end
      notifyPublish(message.getRequestPayload(), status)
    end
    
    def commandCreateResponse (this, context, message, status)
      if (status != MBusClientCommandStatus::SUCCESS)
        if (status == MBusClientCommandStatus::INTERNAL_ERROR)
          notifyConnect(MBusClientConnectStatus::SERVER_ERROR)
        elsif (status == MBusClientCommandStatus::TIMEOUT)
          notifyConnect(MBusClientConnectStatus::TIMEOUT)
        elsif (status == MBusClientCommandStatus::CANCELED)
          notifyConnect(MBusClientConnectStatus::CANCELED)
        else
          notifyConnect(MBusClientConnectStatus::SERVER_ERROR)
        end
        reset()
        @state = MBusClientState::DISCONNECTED
        notifyDisonnect(MBusClientDisconnectStatus::INTERNAL_ERROR)
        return
      end
      if (message.getResponseStatus() != 0)
        notifyConnect(MBusClientConnectStatus::SERVER_ERROR)
        reset()
        @state = MBusClientState::DISCONNECTED
        notifyDisonnect(MBusClientDisconnectStatus::INTERNAL_ERROR)
        return
      end
      payload = message.getResponsePayload()
      if (payload == nil)
        notifyConnect(MBusClientConnectStatus::SERVER_ERROR)
        reset()
        @state = MBusClientState::DISCONNECTED
        notifyDisonnect(MBusClientDisconnectStatus::INTERNAL_ERROR)
        return
      end
      @identifier = payload["identifier"]
      if (@identifier == nil)
        notifyConnect(MBusClientConnectStatus::SERVER_ERROR)
        reset()
        @state = MBusClientState::DISCONNECTED
        notifyDisonnect(MBusClientDisconnectStatus::INTERNAL_ERROR)
        return
      end
      @pingInterval = payload["ping"]["interval"]
      @pingTimeout = payload["ping"]["timeout"]
      @pingThreshold = payload["ping"]["threshold"]
      @compression = payload["compression"]
      @state = MBusClientState::CONNECTED
      notifyConnect(MBusClientConnectStatus::SUCCESS)
    end
    
    def commandCreateRequest
      payload = {}
      if (@options.identifier != nil)
        payload["identifier"] = @options.identifier
      end
      if (@options.pingInterval > 0)
        payload["ping"] = {}
        payload["ping"]["interval"] = @options.pingInterval
        payload["ping"]["timeout"] = @options.pingTimeout
        payload["ping"]["threshold"] = @options.pingThreshold
      end
      payload["compressions"] = []
      payload["compressions"].push("none")
      rc = command(MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_CREATE, payload, method(:commandCreateResponse))
      if (rc != 0)
          return -1
      end
      return 0
    end

    def runConnect
      status = MBusClientConnectStatus::INTERNAL_ERROR
      reset()
      if (@options.serverProtocol == "tcp")
        @socket = Socket.new(AF_INET, SOCK_STREAM, 0)
        sockaddr = Socket.sockaddr_in(@options.serverPort, @options.serverAddress)
        begin
          @socket.connect_nonblock(sockaddr)
          @socketConnected = 1
          status = MBusClientConnectStatus::SUCCESS
        rescue Errno::EINPROGRESS
          status = MBusClientConnectStatus::SUCCESS
        rescue Errno::ECONNREFUSED
          status = MBusClientConnectStatus::CONNECTION_REFUSED
        rescue Errno::ENOENT
          status = MBusClientConnectStatus::SERVER_UNAVAILABLE
        rescue
          status = MBusClientConnectStatus::INTERVAL_ERROR
        end
      else
          status = MBusClientConnectStatus::INVALID_PROTOCOL
      end

      if (status == MBusClientConnectStatus::SUCCESS)
        if (@socketConnected == 1)
          commandCreateRequest()
        end
        return 0
      elsif (status == MBusClientConnectStatus::CONNECTION_REFUSED or
             status == MBusClientConnectStatus::SERVER_UNAVAILABLE)
        notifyConnect(status)
        reset()
        if (@options.connectInterval > 0)
          @state = MBusClientState::CONNECTING
        else
          @state = MBusClientState::DISCONNECTED
          notifyDisonnect(MBusClientDisconnectStatus::CANCELED)
        end
        return 0
      else
        notifyConnect(status)
        reset()
        return -1
      end
    end

    def handleResult (object)
      pending = nil
      sequence = object[MBUS_METHOD_TAG_SEQUENCE]
      if (sequence == nil)
        return -1
      end
      @pendings.each do |p|
        if (p.sequence == sequence)
          pending = p
          break
        end
      end
      if (pending == nil)
        return -1
      end
      @pendings.delete(pending)
      notifyCommand(pending, object, MBusClientCommandStatus::SUCCESS)
      return 0
    end

    def handleEvent (object)
      source = object[MBUS_METHOD_TAG_SOURCE]
      if (source == nil)
        return -1
      end
      identifier = object[MBUS_METHOD_TAG_IDENTIFIER]
      if (identifier == nil)
        return -1
      end
      if (source == MBUS_SERVER_IDENTIFIER and
          identifier == MBUS_SERVER_EVENT_PONG)
        @pingWaitPong = 0
        @pingMissedCount = 0
        @pongRecvTsms = MBusClientClock::get()
      else
        callback = @options.onMessage
        callbackContext = @options.onContext
        for s in @subscriptions
          if ((s.source == MBUS_METHOD_EVENT_SOURCE_ALL or s.source == source) and
              (s.identifier == MBUS_METHOD_EVENT_IDENTIFIER_ALL or s.identifier == identifier))
            if (s.callback != nil)
              callback = s.callback
              callbackContext = s.context
            end
            break
          end
        end
      end
      if (callback != nil)
        message = MBusClientMessageEvent(object)
        callback(self, callbackContext, message)
      end
    end
    
    def wakeUp (reason)
      data = [reason.to_i()].pack("N")
      @wakeupWrite.write(data)
    end
    
    public
    
    attr_reader   :identifier

    def initialize (options = nil)
      @options         = nil
      @state           = MBusClientState::DISCONNECTED
      @socket          = nil
      @requests        = Array.new()
      @pendings        = Array.new()
      @routines        = Array.new()
      @subscriptions   = Array.new()
      @incoming        = String.new()
      @outgoing        = String.new()
      @identifier      = nil
      @connectTsms     = 0
      @pingInterval    = nil
      @pingTimeout     = nil
      @pingThreshold   = nil
      @pingSendTsms    = nil
      @pongRecvTsms    = nil
      @pingWaitPong    = nil
      @pongMissedCount = nil
      @compression     = nil
      @socketConnected = nil
      @sequence        = nil
      @wakeupRead, @wakeupWrite = IO::pipe()
      
      if (options == nil)
        @options = MBusClientOptions.new()
      elsif (!options.is_a?(MBusClientOptions))
        raise "options is invalid"
      else
        @options = options.clone
      end
      
      if (@options.identifier == nil)
        @options.identifier = MBusClientDefaults::IDENTIFIER
      end
      if (@options.connectTimeout == nil or
          @options.connectTimeout <= 0)
        @options.connectTimeout = MBusClientDefaults::CONNECT_TIMEOUT
      end
      if (@options.connectInterval == nil or
          @options.connectInterval <= 0)
        @options.connectInterval = MBusClientDefaults::CONNECT_INTERVAL
      end
      if (@options.subscribeTimeout == nil or
          @options.subscribeTimeout <= 0)
        @options.subscribeTimeout = MBusClientDefaults::SUBSCRIBE_TIMEOUT
      end
      if (@options.registerTimeout == nil or
          @options.registerTimeout <= 0)
        @options.registerTimeout = MBusClientDefaults::REGISTER_TIMEOUT
      end
      if (@options.commandTimeout == nil or
          @options.commandTimeout <= 0)
        @options.commandTimeout = MBusClientDefaults::COMMAND_TIMEOUT
      end
      if (@options.publishTimeout == nil or
          @options.publishTimeout <= 0)
        @options.publishTimeout = MBusClientDefaults::PUBLISH_TIMEOUT
      end

      if (@options.pingInterval == nil or
          @options.pingInterval == 0)
        @options.pingInterval = MBusClientDefaults::PING_INTERVAL
      end

      if (@options.pingTimeout == nil or
          @options.pingTimeout == 0)
        @options.pingTimeout = MBusClientDefaults::PING_TIMEOUT
      end

      if (@options.pingThreshold == nil or
          @options.pingThreshold == 0)
        @options.pingThreshold = MBusClientDefaults::PING_THRESHOLD
      end

      if (@options.serverProtocol == nil)
        @options.serverProtocol = MBusClientDefaults::SERVER_PROTOCOL
      end
      
      if (@options.serverProtocol == MBusClientDefaults::SERVER_TCP_PROTOCOL)
        if (@options.serverAddress == nil)
          @options.serverAddress = MBusClientDefaults::SERVER_TCP_ADDRESS
        end
        if (@options.serverPort == nil or
            @options.serverPort <= 0)
          @options.serverPort = MBusClientDefaults::SERVER_TCP_PORT
        end
      else
        raise "invalid server protocol"
      end
    end
    
    def getOptions
      options = nil
      options = @options
      return options
    end
    
    def getState
      state = nil
      state = @state
      return state
    end
    
    def getIdentifier
      identifier = nil
      identifier = @identifier
      return identifier
    end
    
    def getWakeUpFd
      fd = nil
      fd = @wakeupRead
      return fd
    end
    
    def getWakeUpFdEvents
      raise "not implemented yet"
    end
    
    def getConnectionFd
      fd = nil
      fd = @socket
      return fd
    end
    
    def getConnectionFdEvents
      raise "not implemented yet"
    end
    
    def hasPending
      rc = 0
      if (@requests.count() > 0 or
          @pendings.count() > 0 or
          @incoming.bytesize() > 0 or
          @outgoing.bytesize() > 0)
          rc = 1
      end
      return rc
    end
    
    def connect
      if (@state != MBusClientState::CONNECTED)
        @state = MBusClientState::CONNECTING
        wakeUp(MBusClientWakeUpReason::CONNECT)
      end
    end
    
    def disconnect
      if (@state != MBusClientState::DISCONNECTED)
        @state = MBusClientState::DISCONNECTING
        wakeUp(MBusClientWakeUpReason::DISCONNECT)
      end
    end
    
    def subscribe (event, callback = nil, context = nil, source = nil, timeout = nil)
      if (@state != MBusClientState::CONNECTED)
        raise "client state is not connected: %d" % [@state]
      end
      if (source == nil)
        source = MBUS_METHOD_EVENT_SOURCE_ALL
      end
      if (event == nil)
        raise "event is invalid"
      end
      for subscription in @subscriptions
        if (subscription.source == source and
            subscription.identifier == event)
          raise "subscription already exists"
        end
      end
      if (timeout == nil or
          timeout < 0)
        timeout = @options.subscribeTimeout
      end
      subscription = MBusClientSubscription.new(source, event, callback, context)
      if (subscription == nil)
        raise "can not create subscription"
      end
      payload = {}
      payload["source"] = source
      payload["event"] = event
      rc = self.command(MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_SUBSCRIBE, payload, method(:commandSubscribeResponse), subscription, timeout)
      if (rc != 0)
          raise "can not call subscribe command"
      end
      return 0
    end
    
    def unsubscribe
      if (@state != MBusClientState::CONNECTED)
        raise "client state is not connected: %d" % [@state]
      end
      if (source == nil)
        source = MBUS_METHOD_EVENT_SOURCE_ALL
      end
      if (event == nil)
        raise "event is invalid"
      end
      subscription = nil
      for s in @subscriptions
        if (s.source == source and
            s.identifier == event)
          subscription = s
          break
        end
      end
      if (subscription != nil)
        raise "can not find subscription for source: %s, event: %s" % [ source, event ]
      end
      payload = {}
      payload["source"] = source
      payload["event"] = event
      rc = command(MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_UNSUBSCRIBE, payload, method(:commandUnsubscribeResponse), subscription, timeout)
      if (rc != 0)
        raise "can not call unsubscribe command"
      end
      return 0
    end
    
    def publish (event, payload = nil, qos = nil, destination = nil, timeout = nil)
      if (@state != MBusClientState::CONNECTED)
        raise "client state is not connected: %d" % [@state]
      end
      if (event == nil)
        raise "event is invalid"
      end
      if (qos == nil)
        qos = MBusClientQoS::AT_MOST_ONCE
      end
      if (destination == nil)
        destination = MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS
      end
      if (timeout == nil or
          timeout <= 0)
        timeout = @options.publishTimeout
      end
      if (qos == MBusClientQoS::AT_MOST_ONCE)
        request = MBusClientRequest.new(MBUS_METHOD_TYPE_EVENT, destination, event, @sequence, payload, nil, nil, timeout)
        if (request == nil)
          raise "can not create request"
        end
        @sequence += 1
        if (@sequence >= MBUS_METHOD_SEQUENCE_END)
          @sequence = MBUS_METHOD_SEQUENCE_START
        end
        @requests.push(request)
      elsif (qos == MBusClientQoS::AT_LEAST_ONCE)
        cpayload = {}
        cpayload[MBUS_METHOD_TAG_DESTINATION] = destination
        cpayload[MBUS_METHOD_TAG_IDENTIFIER] = event
        cpayload[MBUS_METHOD_TAG_PAYLOAD] = payload
        command(MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_EVENT, cpayload, method(:commandEventResponse), nil, timeout)
      else
        raise "qos: %d is invalid" % [ qos ]
      end
      return 0
    end
    
    def register
      raise "not implemented yet"
    end
    
    def unregister
      raise "not implemented yet"
    end
    
    def command (destination, command, payload = nil, callback = nil, context = nil, timeout = nil)
      if (destination == nil)
        raise "destination is invalid"
      end
      if (command == nil)
        raise "command is invalid"
      end
      if (command == MBUS_SERVER_COMMAND_CREATE)
        if (@state != MBusClientState::CONNECTING)
          raise "client state is not connecting: %d" % [@state]
        end
      else
        if (@state != MBusClientState::CONNECTED)
          raise "client state is not connected: %d" % [@state]
        end
      end
      if (timeout == nil or
          timeout <= 0)
        timeout = @options.commandTimeout
      end
      request = MBusClientRequest.new(MBUS_METHOD_TYPE_COMMAND, destination, command, @sequence, payload, callback, context, timeout)
      if (request == nil)
        raise "can not create request"
      end
      @sequence += 1
      if (@sequence >= MBUS_METHOD_SEQUENCE_END)
        @sequence = MBUS_METHOD_SEQUENCE_START
      end
      @requests.push(request)
      return 0
    end
    
    def getRunTimeout
      current = MBusClientClock::get()
      timeout = MBusClientDefaults::RUN_TIMEOUT
      if (@state == MBusClientState::CONNECTING)
        if (@socket == nil)
          if (MBusClientClock::after(current, @connectTsms + @options.connectInterval))
            timeout = 0
          else
            timeout = [timeout, (@connectTsms + @options.connectInterval) - (current)].min
          end
        elsif (@socketConnected == 0)
          if (MBusClientClock::after(current, @connectTsms + @options.connectTimeout))
            timeout = 0
          else
            timeout = [timeout, (@connectTsms + @options.connectTimeout) - (current)].min
          end
        end
      elsif (@state == MBusClientState::CONNECTED)
        if (@pingInterval > 0)
          if (MBusClientClock::after(current, @pingSendTsms + @pingInterval))
            timeout = 0
          else
            timeout = [timeout, (@pingSendTsms + @pingInterval) - (current)].min
          end
        end
        for request in @requests
          if (request.timeout >= 0)
            if (MBusClientClock::after(current, request.createdAt + request.timeout))
              timeout = 0
            else
              timeout = [timeout, (request.createdAt + request.timeout) - (current)].min
            end
          end
        end
        for request in @pendings
          if (request.timeout >= 0)
            if (MBusClientClock::after(current, request.createdAt + request.timeout))
              timeout = 0
            else
              timeout = [timeout, (request.createdAt + request.timeout) - (current)].min
            end
          end
        end
        if (@incoming.bytesize() > 0)
          timeout = 0
        end
      elsif (@state == MBusClientState::DISCONNECTING)
        timeout = 0
      elsif (@state == MBusClientState::DISCONNECTED)
        if (@options.connectInterval > 0)
          if (MBusClientClock::after(current, @connectTsms + @options.connectInterval))
            timeout = 0
          else
            timeout = [timeout, (@connectTsms + @options.connectInterval) - (current)].min
          end
        end
      end
      return timeout
    end
    
    def breakRun
      raise "not implemented yet"
    end
    
    def run (timeout = -1)
      if (@state == MBusClientState::CONNECTING)
        if (@socket == nil)
          current = MBusClientClock::get()
          if (@options.connectInterval <= 0 or
            MBusClientClock::after(current, @connectTsms + @options.connectInterval))
            @connectTsms = MBusClientClock::get()
            rc = runConnect()
            if (rc != 0)
              raise "can not connect client"
            end
          end
        end
      elsif (@state == MBusClientState::CONNECTED)
      elsif (@state == MBusClientState::DISCONNECTING)
        reset()
        @state = MBusClientState::DISCONNECTED
        notifyDisonnect(MBusClientDisconnectStatus::SUCCESS)
        return 0
      elsif (@state == MBusClientState::DISCONNECTED)
        if (@options.connectInterval > 0)
          @state = MBusClientState::CONNECTING
          return 0
        end
      else
        raise "client state: %d is invalid" % [@state]
      end
      
      selectRead = Array.new()
      selectWrite = Array.new()
      
      selectRead.push(@wakeupRead)
      if (@socket != nil)
        if (@state == MBusClientState::CONNECTING and
            @socketConnected == 0)
          selectWrite.push(@socket)
        else
          selectRead.push(@socket)
          if (@outgoing.bytesize() > 0)
            selectWrite.push(@socket)
          end
        end
      end

      ptimeout = getRunTimeout()
      if (ptimeout < 0 or
          timeout < 0)
          ptimeout = [ptimeout, timeout].max
      else
          ptimeout = [ptimeout, timeout].min
      end

      selectReadable, selectWritable, = IO.select(selectRead, selectWrite, nil, ptimeout / 1000.00)
      
      if (selectReadable != nil)
        selectReadable.each do |fd|
          if (fd == @wakeupRead)
            data = @wakeupRead.read(4)
            reason = data.unpack("N")[0]
          end
          if (fd == @socket)
            data = String.new()
            begin
              data = @socket.read_nonblock(4096)
            rescue Errno::EAGAIN
            rescue Errno::EINTR
            rescue Errno::EWOULDBLOCK
            rescue
              raise "recv failed"
            end
            if (data.bytesize() <= 0)
              reset()
              @state = MBusClientState::DISCONNECTED
              notifyDisonnect(MBusClientDisconnectStatus::CONNECTION_CLOSED)
              return 0
            end
            @incoming += data
          end
        end
      end
      
      if (selectWritable != nil)
        selectWritable.each do |fd|
          if (fd == @socket)
            if (@state == MBusClientState::CONNECTING and
                @socketConnected == 0)
              sockaddr = Socket.sockaddr_in(@options.serverPort, @options.serverAddress)
              begin
                @socket.connect_nonblock(sockaddr)
              rescue Errno::EISCONN
                @socketConnected = 1
                commandCreateRequest()
              rescue Errno::ECONNREFUSED
                notifyConnect(MBusClientConnectStatus::CONNECTION_REFUSED)
                reset()
                if (@options.connectInterval > 0)
                  @state = MBusClientState::CONNECTING
                else
                  @state = MBusClientState::DISCONNECTED
                  notifyDisonnect(MBusClientDisconnectStatus::CANCELED)
                end
                return 0
              rescue
                notifyConnect(MBusClientConnectStatus::INTERNAL_ERROR)
                raise "can not connect to server"
              end
            elsif (@outgoing.bytesize() > 0)
              dlen = 0
              begin
                dlen = @socket.write_nonblock(@outgoing)
              rescue Errno::EAGAIN
              rescue Errno::EINTR
              rescue Errno::EWOULDBLOCK
              rescue
                raise "send failed"
              end
              if (dlen > 0)
                @outgoing.slice!(0, dlen)
              end
            end
          end
        end
      end
      
      current = MBusClientClock::get()
      
      while (@incoming.bytesize() >= 4)
        dlen = @incoming.slice(0, 4)
        dlen = dlen.unpack("N")[0]
        if (dlen > @incoming.bytesize() - 4)
          break
        end
        slice = @incoming.slice(4, dlen)
        @incoming.slice!(0, 4 + dlen)
        object = JSON.parse(slice)
        if (object[MBUS_METHOD_TAG_TYPE] == MBUS_METHOD_TYPE_RESULT)
          handleResult(object)
        elsif (object[MBUS_METHOD_TAG_TYPE] == MBUS_METHOD_TYPE_EVENT)
          handleEvent(object)
        else
          raise "unknown type: %s" % [object[MBUS_METHOD_TAG_TYPE]]
        end
      end
      
      if (@state == MBusClientState::CONNECTING and
          @socket != nil and
          @socketConnected == 0)
        if (MBusClientClock::after(current, @connectTsms + @options.connectTimeout))
          notifyConnect(MBusClientConnectStatus::TIMEOUT)
          reset()
          if (@options.connectInterval > 0)
            @state == MBusClientState::CONNECTING
          else
            @state == MBusClientState::DISCONNECTED
          end
          return 0
        end
      end
      
      if (@state == MBusClientState::CONNECTED and
          @pingInterval > 0)
        if (MBusClientClock::after(current, @pingSendTsms + @pingInterval))
          @pingSendTsms = current
          @pongRecvTsms = 0
          @pingWaitPong = 1
          rc = publish(MBUS_SERVER_EVENT_PING, nil, nil, MBUS_SERVER_IDENTIFIER, nil)
          if (rc != 0)
            raise "can not publish ping"
          end
        end
        if (@pingWaitPong != 0 and
            @pingSendTsms != 0 and
            @pongRecvTsms == 0 and
            MBusClientClock::after(current, @pingSendTsms + @pingTimeout))
          @pingWaitPong = 0
          @pongMissedCount += 1
        end
        if (@pongMissedCount > @pingThreshold)
          notifyDisonnect(MBusClientDisconnectStatus::PING_TIMEOUT)
          reset()
          if (@options.connectInterval > 0)
            @state = MBusClientState::CONNECTING
          else
            @state = MBusClientState::DISCONNECTED
          end
          return 0
        end
      end

      @requests.delete_if do |request|
        if (request.timeout < 0 or
          MBusClientClock::before(current, request.createdAt + request.timeout))
            next false
        end
        if (request.type == MBUS_METHOD_TYPE_EVENT)
            if (request.destination != MBUS_SERVER_IDENTIFIER and
                request.identifier != MBUS_SERVER_EVENT_PING)
              notifyPublish(json.loads(request.stringify()), MBusClientPublishStatus::TIMEOUT)
            end
        elsif (request.type == MBUS_METHOD_TYPE_COMMAND)
            if (request.identifier == MBUS_SERVER_COMMAND_EVENT)
              notifyPublish(request.payload, MBusClientPublishStatus::TIMEOUT)
            elsif (request.identifier == MBUS_SERVER_COMMAND_SUBSCRIBE)
              notifySubscribe(request.payload["source"], request.payload["event"], MBusClientSubscribeStatus::TIMEOUT)
            elsif (request.identifier == MBUS_SERVER_COMMAND_UNSUBSCRIBE)
              notifyUnsubscribe(request.payload["source"], request.payload["event"], MBusClientUnsubscribeStatus::TIMEOUT)
            elsif (request.identifier == MBUS_SERVER_COMMAND_REGISTER)
              notifyRegistered(request.payload["command"], MBusClientRegisterStatus::TIMEOUT)
            elsif (request.identifier == MBUS_SERVER_COMMAND_UNREGISTER)
              notifyUnregistered(request.payload["command"], MBusClientUnregisterStatus::TIMEOUT)
            else
              notifyCommand(request, nil, MBusClientCommandStatus::TIMEOUT)
            end
        end
        next true
      end
      
      @pendings.delete_if do |request|
        if (request.timeout < 0 or
          MBusClientClock::before(current, request.createdAt + request.timeout))
            next false
        end
        if (request.type == MBUS_METHOD_TYPE_EVENT)
            if (request.destination != MBUS_SERVER_IDENTIFIER and
                request.identifier != MBUS_SERVER_EVENT_PING)
              notifyPublish(json.loads(request.stringify()), MBusClientPublishStatus::TIMEOUT)
            end
        elsif (request.type == MBUS_METHOD_TYPE_COMMAND)
            if (request.identifier == MBUS_SERVER_COMMAND_EVENT)
              notifyPublish(request.payload, MBusClientPublishStatus::TIMEOUT)
            elsif (request.identifier == MBUS_SERVER_COMMAND_SUBSCRIBE)
              notifySubscribe(request.payload["source"], request.payload["event"], MBusClientSubscribeStatus::TIMEOUT)
            elsif (request.identifier == MBUS_SERVER_COMMAND_UNSUBSCRIBE)
              notifyUnsubscribe(request.payload["source"], request.payload["event"], MBusClientUnsubscribeStatus::TIMEOUT)
            elsif (request.identifier == MBUS_SERVER_COMMAND_REGISTER)
              notifyRegistered(request.payload["command"], MBusClientRegisterStatus::TIMEOUT)
            elsif (request.identifier == MBUS_SERVER_COMMAND_UNREGISTER)
              notifyUnregistered(request.payload["command"], MBusClientUnregisterStatus::TIMEOUT)
            else
              notifyCommand(request, nil, MBusClientCommandStatus::TIMEOUT)
            end
        end
        next true
      end
      
      while (@requests.count() > 0)
        request = @requests.shift()
        
        data = request.stringify()
        data = data.to_s().encode("UTF-8")
        data.force_encoding("ASCII-8BIT")
        dlen = [data.bytesize().to_i()].pack("N")
        
        @outgoing += dlen
        @outgoing += data

        if (request.type == MBUS_METHOD_TYPE_EVENT)
          if (request.destination != MBUS_SERVER_IDENTIFIER and
              request.identifier != MBUS_SERVER_EVENT_PING)
            notifyPublish(json.loads(request.stringify()), MBusClientPublishStatus::SUCCESS)
          end
        else
          @pendings.push(request)
        end
      end
    end
  end
end
