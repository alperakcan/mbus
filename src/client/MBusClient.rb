
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

require 'socket'
include Socket::Constants

module MBusClient
  MBUS_METHOD_SEQUENCE_START                  = 1
  MBUS_METHOD_SEQUENCE_END                    = 9999

  class MBusClientClock
    def self.get
      return Process.clock_gettime(Process::CLOCK_MONOTONIC_RAW, :millisecond)
    end
    def self.after (a, b)
      return ((((b) - (a)) < 0)) ? 1 : 0;
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
    INVALID_IDENTIFIER = 8
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
      @onRoutine        = nil
      @onPublish        = nil
      @onSubscribe      = nil
      @onUnsubscribe    = nil
      @onRegistered     = nil
      @onUnregistered   = nil
      @onContext        = nil
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
    attr_accessor :identifier
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
    
    def notifyConnect (status)
      if (@options.onConnect != nil)
        @options.onConnect.call(@options.onContext, status)
      end
    end

    def notifyDisonnect (status)
      if (@options.onDisconnect != nil)
        @options.onDisconnect.call(@options.onContext, status)
      end
    end

    def reset
      if (@socket != nil)
        @socket.close()
        @socket = nil
      end
      @identifier      = nil
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
          @state = MBusClientState.Disconnected
          notifyDisonnect(MBusClientDisconnectStatus.Canceled)
        end
        return 0
      else
        notifyConnect(status)
        reset()
        return -1
      end
    end

    public
    
    def initialize (options = nil)
      @options         = nil
      @state           = MBusClientState::DISCONNECTED
      @socket          = nil
      @requests        = nil
      @pendings        = nil
      @routines        = nil
      @subscriptions   = nil
      @incoming        = nil
      @outgoing        = nil
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
      raise "not implemented yet"
    end
    
    def getWakeUpFdEvents
      raise "not implemented yet"
    end
    
    def getConnectionFd
      raise "not implemented yet"
    end
    
    def getConnectionFdEvents
      raise "not implemented yet"
    end
    
    def hasPending
      raise "not implemented yet"
    end
    
    def connect
      if (@state != MBusClientState::CONNECTED)
        @state = MBusClientState::CONNECTING
      end
    end
    
    def disconnect
      raise "not implemented yet"
    end
    
    def subscribe
      raise "not implemented yet"
    end
    
    def unsubscribe
      raise "not implemented yet"
    end
    
    def publish
      raise "not implemented yet"
    end
    
    def register
      raise "not implemented yet"
    end
    
    def unregister
      raise "not implemented yet"
    end
    
    def command
      raise "not implemented yet"
    end
    
    def getRunTimeout
      raise "not implemented yet"
    end
    
    def breakRun
      raise "not implemented yet"
    end
    
    def run (timeout = -1)
      p"loop"
      if (@state == MBusClientState::CONNECTING)
        p"state == CONNECTING"
        if (@socket == nil)
          current = MBusClientClock::get()
          if (@options.connectInterval <= 0 or
            MBusClientClock::after(current, @connectTsms + @options.connectInterval) != 0)
            @connectTsms = MBusClientClock::get()
            rc = runConnect()
            if (rc != 0)
              raise "can not connect client"
            end
          end
        end
      elsif (@state == MBusClientState::CONNECTED)
        p"state == CONNECTED"
        pass
      elsif (@state == MBusClientState::DISCONNECTING)
        p"state == DISCONNECTING"
        reset()
        @state = MBusClientState::DISCONNECTED
        notifyDisonnect(MBusClientDisconnectStatus::SUCCESS)
        return 0
      elsif (@state == MBusClientState::DISCONNECTED)
        p"state == DISCONNECTED"
        if (@options.connectInterval > 0)
          @state = MBusClientState::CONNECTING
          return 0
        end
      else
        raise ValueError("client state: {} is invalid".format(@state))
      end
      
      selectRead = Array.new()
      selectWrite = Array.new()
      if (@socket != nil)
        if (@state == MBusClientState::CONNECTING and
            @socketConnected == 0)
          selectWrite.push(@socket)
        else
          selectRead.push(@socket)
        end
      end

      selectReadable, selectWritable, = IO.select(selectRead, selectWrite, nil, 1000 / 1000.00)
      
      if (selectWritable != nil)
        selectWritable.each{ |fd|
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
                notifyConnect(MBusClientConnectStatus.InternalError)
                raise "can not connect to server"
              end
            end
          end 
        }
      end
#      raise "not implemented yet"
    end
  end
end

def onConnect (context, status)
  puts "connect status: %s" % MBusClient::MBusClientConnectStatus::string(status)
end

def onDisconnect (context, status)
  puts "disconnect status: %s" % MBusClient::MBusClientDisconnectStatus::string(status)
end

options = MBusClient::MBusClientOptions.new()
options.connectInterval = 0
options.onConnect = method(:onConnect)
options.onDisconnect = method(:onDisconnect)

client = MBusClient::MBusClient.new(options)
client.connect()

while (1)
  client.run()
end
