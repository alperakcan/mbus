
module MBusClient
  class MBusClientDefaults
    CLIENT_IDENTIFIER   = nil
    
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
    INVALID_CLIENT_IDENTIFIER = 8
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
      elsif (status == INVALID_CLIENT_IDENTIFIER)
        return "invalid client identifier"
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
end

puts MBusClient::MBusClientConnectStatus::INVALID_CLIENT_IDENTIFIER
puts MBusClient::MBusClientPublishStatus::string(MBusClient::MBusClientPublishStatus::TIMEOUT)

options = MBusClient::MBusClientOptions.new
puts options.inspect
