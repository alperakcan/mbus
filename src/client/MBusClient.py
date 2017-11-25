
import ctypes
import time
import errno
import socket
import json
import collections
import select
import struct

MBUS_METHOD_TYPE_COMMAND                    = "org.mbus.method.type.command"
MBUS_METHOD_TYPE_EVENT                      = "org.mbus.method.type.event"
MBUS_METHOD_TYPE_RESULT                     = "org.mbus.method.type.result"

MBUS_METHOD_SEQUENCE_START                  = 1
MBUS_METHOD_SEQUENCE_END                    = 9999

MBUS_METHOD_EVENT_SOURCE_ALL                = "org.mbus.method.event.source.all"

MBUS_METHOD_EVENT_DESTINATION_ALL           = "org.mbus.method.event.destination.all"
MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS   = "org.mbus.method.event.destination.subscribers"

MBUS_METHOD_EVENT_IDENTIFIER_ALL            = "org.mbus.method.event.identifier.all"

MBUS_METHOD_STATUS_IDENTIFIER_ALL           = "org.mbus.method.event.status.all"

MBUS_SERVER_IDENTIFIER                      = "org.mbus.server"

MBUS_SERVER_COMMAND_CREATE                  = "command.create"
MBUS_SERVER_COMMAND_EVENT                   = "command.event"
MBUS_SERVER_COMMAND_CALL                    = "command.call"
MBUS_SERVER_COMMAND_RESULT                  = "command.result"
MBUS_SERVER_COMMAND_STATUS                  = "command.status"
MBUS_SERVER_COMMAND_CLIENTS                 = "command.clients"
MBUS_SERVER_COMMAND_SUBSCRIBE               = "command.subscribe"
MBUS_SERVER_COMMAND_REGISTER                = "command.register"
MBUS_SERVER_COMMAND_UNSUBSCRIBE             = "command.unsubscribe"
MBUS_SERVER_COMMAND_CLOSE                   = "command.close"

MBUS_SERVER_STATUS_CONNECTED                = "status.connected"
MBUS_SERVER_STATUS_DISCONNECTED             = "status.disconnected"
MBUS_SERVER_STATUS_SUBSCRIBED               = "status.subscribed"
MBUS_SERVER_STATUS_SUBSCRIBER               = "status.subscriber"
MBUS_SERVER_STATUS_UNSUBSCRIBED             = "status.unsubscribed"

MBUS_SERVER_EVENT_CONNECTED                 = "event.connected"
MBUS_SERVER_EVENT_DISCONNECTED              = "event.disconnected"
MBUS_SERVER_EVENT_SUBSCRIBED                = "event.subscribed"
MBUS_SERVER_EVENT_UNSUBSCRIBED              = "event.unsubscribed"

MBUS_SERVER_EVENT_PING                      = "event.ping"
MBUS_SERVER_EVENT_PONG                      = "event.pong"

try:
    time_func = time.monotonic
except AttributeError:
    time_func = time.time

class MBusClientDefaults:
    ClientIdentifier = None
    
    ServerTCPProtocol = "tcp"
    ServerTCPAddress  = "127.0.0.1"
    ServerTCPPort     = 8000
    
    ServerProtocol    = ServerTCPProtocol
    ServerAddress     = ServerTCPAddress
    ServerPort        = ServerTCPPort

    RunTimeout        = 250
    
    ConnectTimeout    = 30000
    ConnectInterval   = 0
    SubscribeTimeout  = 30000
    RegisterTimeout   = 30000
    CommandTimeout    = 30000
    Publishtimeout    = 30000
    
    PingInterval      = 180000
    PingTimeout       = 5000
    PingThreshold     = 2

class MBusClientQoS:
    AtMostOnce  = 0
    AtLeastOnce = 1
    ExactlyOnce = 2

class MBusClientState:
    Unknown       = 0
    Connecting    = 1
    Connected     = 2
    Disconnecting = 3
    Disconnected  = 4

class MBusClientConnectStatus:
    Success                 = 0
    InternalError           = 1
    InvalidProtocol         = 2
    ConnectionRefused       = 3
    ServerUnavailable       = 4
    Timeout                 = 5
    InvalidProtocolVersion  = 6
    InvalidClientIdentifier = 7
    ServerError             = 8

class MBusClientDisconnectStatus:
    Success          = 0
    InternalError    = 1
    ConnectionClosed = 2

class MBusClientPublishStatus:
    Success       = 0
    InternalError = 1
    Timeout       = 2
    Canceled      = 3

class MBusClientSubscribeStatus:
    Success       = 0
    InternalError = 1
    Timeout       = 2
    Canceled      = 3

class MBusClientUnsubscribeStatus:
    Success       = 0
    InternalError = 1
    Timeout       = 2
    Canceled      = 3

class MBusClientRegisterStatus:
    Success       = 0
    InternalError = 1
    Timeout       = 2
    Canceled      = 3

class MBusClientUnregisterStatus:
    Success       = 0
    InternalError = 1
    Timeout       = 2
    Canceled      = 3

class MBusClientCommandStatus:
    Success       = 0
    InternalError = 1
    Timeout       = 2
    Canceled      = 3

class MBusClientOptions:
    
    def __init__ (self):
        self.identifier       = None
        self.serverProtocol   = None
        self.serverAddress    = None
        self.serverPort       = None
        self.connectTimeout   = None
        self.connectInterval  = None
        self.subscribeTimeout = None
        self.registerTimeout  = None
        self.commandTimeout   = None
        self.publishTimeout   = None
        self.pingInterval     = None
        self.pingTimeout      = None
        self.pingThreshold    = None
        
        self.onConnect        = None
        self.onDisconnect     = None
        self.onMessage        = None
        self.onRoutine        = None
        self.onPublish        = None
        self.onSubscribe      = None
        self.onUnsubscribe    = None
        self.onRegistered     = None
        self.onUnregistered   = None
        self.onContext        = None
    
    def __str__ (self):
        options = {}
        
        options['identifier']       = self.identifier
        options['serverProtocol']   = self.serverProtocol
        options['serverAddress']    = self.serverAddress
        options['serverPort']       = self.serverPort
        options['connectTimeout']   = self.connectTimeout
        options['connectInterval']  = self.connectInterval
        options['subscribeTimeout'] = self.subscribeTimeout
        options['registerTimeout']  = self.registerTimeout
        options['commandTimeout']   = self.commandTimeout
        options['publishTimeout']   = self.publishTimeout
        options['pingInterval']     = self.pingInterval
        options['pingTimeout']      = self.pingTimeout
        options['pingThreshold']    = self.pingThreshold

        options['onConnect']        = self.onConnect
        options['onDisconnect']     = self.onDisconnect
        options['onMessage']        = self.onMessage
        options['onRoutine']        = self.onRoutine
        options['onPublish']        = self.onPublish
        options['onSubscribe']      = self.onSubscribe
        options['onUnsubscribe']    = self.onUnsubscribe
        options['onRegistered']     = self.onRegistered
        options['onUnregistered']   = self.onUnregistered
        options['onContext']        = self.onContext

        return json.dumps(options)

class MBusClientRoutine(object):
    
    def __init__ (self, identifier, callback, context):
        self.identifier = identifier;
        self.callback   = callback;
        self.context    = context;

    def __str__ (self):
        routine = {}
        routine['identifier'] = self.identifier
        routine['callback']   = self.callback
        routine['context']    = self.context
        return json.dumps(routine)

class MBusClientSubscription(object):
    
    def __init__ (self, source, identifier, callback, context):
        self.source     = source;
        self.identifier = identifier;
        self.callback   = callback;
        self.context    = context;

    def __str__ (self):
        subscription = {}
        subscription['source']     = self.source
        subscription['identifier'] = self.identifier
        subscription['callback']   = self.callback
        subscription['context']    = self.context
        return json.dumps(subscription)

class MBusClientRequest(object):

    def __init__ (self, type, destination, identifier, sequence, payload, callback, context, timeout):
        self.type        = type
        self.destination = destination
        self.identifier  = identifier
        self.sequence    = sequence
        self.payload     = payload
        self.callback    = callback
        self.context     = context
        self.timeout     = timeout
    
    def __str__ (self):
        request = {}
        request['type']        = self.type
        request['destination'] = self.destination
        request['identifier']  = self.identifier
        request['sequence']    = self.sequence
        request['payload']     = self.payload
        request['callback']    = self.callback
        request['context']     = self.context
        request['timeout']     = self.timeout
        return json.dumps(request)

class MBusClient(object):
    
    def __notifyConnect (self, status):
        if (self.__options.onConnect != None):
            self.__options.onConnect(self, self.__options.onContext, status)

    def __notifyDisonnect (self, status):
        if (self.__options.onDisconnect != None):
            self.__options.onDisconnect(self, self.__options.onContext, status)

    def __reset (self):
        
        self.__state           = MBusClientState.Disconnected
        if (self.__socket != None):
            self.__socket.close()
            self.__socket = None
        self.__requests.clear()
        self.__pendings.clear()
        self.__routines.clear()
        self.__subscriptions.clear()
        self.__incoming        = ""
        self.__outgoing        = ""
        self.__identifier      = None
        self.__connectTsms     = 0
        self.__pingInterval    = 0
        self.__pingTimeout     = 0
        self.__pingThreshold   = 0
        self.__pingSendTsms    = 0
        self.__pongRecvTsms    = 0
        self.__pingWaitPong    = 0
        self.__pongMissedCount = 0
        self.__compression     = None
        self.__socketConnected = 0
        self.__sequence        = MBUS_METHOD_SEQUENCE_START
    
    def __commandRegisterResponse (self, this, context, message, status):
        raise
    
    def __commandUnregisterResponse (self, this, context, message, status):
        raise
    
    def __commandSubscribeResponse (self, this, context, message, status):
        raise
    
    def __commandUnsubscribeResponse (self, this, context, message, status):
        raise
    
    def __commandEventResponse (self, this, context, message, status):
        raise
    
    def __commandCreateResponse (self, this, context, message, status):
        raise
    
    def __commandCreateRequest (self):
        raise
    
    def __runConnect (self):
        raise
    
    def __handleResult (self, json):
        raise
    
    def __handleEvent (self, json):
        raise
    
    def __handleCommand (self, json):
        raise
    
    def __wakeUp (self, reason):
        raise
    
    def __init__ (self, options = None):
        
        self.__options         = None
        self.__state           = MBusClientState.Disconnected
        self.__socket          = None
        self.__requests        = collections.deque()
        self.__pendings        = collections.deque()
        self.__routines        = collections.deque()
        self.__subscriptions   = collections.deque()
        self.__incoming        = None
        self.__outgoing        = None
        self.__identifier      = None
        self.__connectTsms     = None
        self.__pingInterval    = None
        self.__pingTimeout     = None
        self.__pingThreshold   = None
        self.__pingSendTsms    = None
        self.__pongRecvTsms    = None
        self.__pingWaitPong    = None
        self.__pongMissedCount = None
        self.__compression     = None
        self.__socketConnected = None
        self.__sequence        = None
        self.__mutex           = None

        if (options == None):
            self.__options = MBusClientOptions()
        else:
            self.__options = options
        
        if (self.__options.identifier == None):
            self.__options.identifier = MBusClientDefaults.ClientIdentifier
        
        if (self.__options.connectTimeout == None or
            self.__options.connectTimeout <= 0):
            self.__options.connectTimeout = MBusClientDefaults.ConnectTimeout
        if (self.__options.connectInterval == None or
            self.__options.connectInterval <= 0):
            self.__options.connectInterval = MBusClientDefaults.ConnectInterval
        if (self.__options.subscribeTimeout == None or
            self.__options.subscribeTimeout <= 0):
            self.__options.subscribeTimeout = MBusClientDefaults.SubscribeTimeout
        if (self.__options.registerTimeout == None or
            self.__options.registerTimeout <= 0):
            self.__options.registerTimeout = MBusClientDefaults.RegisterTimeout
        if (self.__options.commandTimeout == None or
            self.__options.commandTimeout <= 0):
            self.__options.commandTimeout = MBusClientDefaults.CommandTimeout
        if (self.__options.publishTimeout == None or
            self.__options.publishTimeout <= 0):
            self.__options.publishTimeout = MBusClientDefaults.Publishtimeout

        if (self.__options.pingInterval == None or
            self.__options.pingInterval == 0):
            self.__options.pingInterval = MBusClientDefaults.PingInterval

        if (self.__options.pingTimeout == None or
            self.__options.pingTimeout == 0):
            self.__options.pingTimeout = MBusClientDefaults.PingTimeout

        if (self.__options.pingThreshold == None or
            self.__options.pingThreshold == 0):
            self.__options.pingThreshold = MBusClientDefaults.PingThreshold

        if (self.__options.serverProtocol == None):
            self.__options.serverProtocol = MBusClientDefaults.ServerProtocol
        
        if (self.__options.serverProtocol == MBusClientDefaults.ServerTCPProtocol):
            if (self.__options.serverAddress == None):
                self.__options.serverAddress = MBusClientDefaults.ServerTCPAddress
            if (self.__options.serverPort == None or
                self.__options.serverPort <= 0):
                self.__options.serverPort = MBusClientDefaults.ServerTCPPort
        else:
            raise ValueError("invalid server protocol: {}".format(self.__options.serverProtocol))
    
    def lock (self):
        raise
    
    def unlock (self):
        raise
    
    def getState (self):
        raise
    
    def getIdentifier (self):
        raise
    
    def getWakeUpFd (self):
        raise
    
    def getWakeUpFdEvents (self):
        raise
    
    def getConnectionFd (self):
        raise
    
    def getConnectionFdEvents (self):
        raise
    
    def hasPending (self):
        raise
    
    def connect (self):
        raise
    
    def disconnect (self):
        raise
    
    def subscribe (self, source, event, callback = None, context = None, timeout = None):
        raise

    def unsubscribe (self, source, event, timeout = None):
        raise
    
    def publish (self, destination, event, payload = None, qos = None, timeout = None):
        raise

    def register (self, command, callback, context, timeout):
        raise
    
    def unregister (self, command):
        raise
    
    def command (self, destination, command, payload, callback, context, timeout = None):
        raise