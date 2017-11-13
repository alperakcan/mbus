
import ctypes
import time
import errno
import socket
import json
import collections
import select
import struct

MBUS_METHOD_TYPE_CREATE                     = "org.mbus.method.type.create"
MBUS_METHOD_TYPE_COMMAND                    = "org.mbus.method.type.command"
MBUS_METHOD_TYPE_STATUS                     = "org.mbus.method.type.status"
MBUS_METHOD_TYPE_EVENT                      = "org.mbus.method.type.event"
MBUS_METHOD_TYPE_RESULT                     = "org.mbus.method.type.result"

MBUS_METHOD_SEQUENCE_START                  = 1
MBUS_METHOD_SEQUENCE_END                    = 9999

MBUS_METHOD_EVENT_SOURCE_ALL                = "org.mbus.method.event.source.all"

MBUS_METHOD_EVENT_DESTINATION_ALL           = "org.mbus.method.event.destination.all"
MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS   = "org.mbus.method.event.destination.subscribers"

MBUS_METHOD_EVENT_IDENTIFIER_ALL            = "org.mbus.method.event.identifier.all"

MBUS_METHOD_STATUS_IDENTIFIER_ALL           = "org.mbus.method.event.status.all"

MBUS_SERVER_NAME                            = "org.mbus.server"

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

# mbus client options default values
MBUS_CLIENT_OPTIONS_DEFAULT_SERVER_PROTOCOL = "tcp"
MBUS_CLIENT_OPTIONS_DEFAULT_SERVER_ADDRESS  = "127.0.0.1"
MBUS_CLIENT_OPTIONS_DEFAULT_SERVER_PORT     = 8000

MBUS_CLIENT_OPTIONS_DEFAULT_CLIENT_NAME     = ""

MBUS_CLIENT_OPTIONS_DEFAULT_PING_INTERVAL   = 180000
MBUS_CLIENT_OPTIONS_DEFAULT_PING_TIMEOUT    = 5000
MBUS_CLIENT_OPTIONS_DEFAULT_PING_THRESHOLD  = 2

# mbus client loop default timeout in milliseconds
MBUS_CLIENT_RUN_TIMEOUT                     = 250

# mbus client states
MBUS_CLIENT_STATE_INITIAL                   = 0
MBUS_CLIENT_STATE_CONNECTING                = 1
MBUS_CLIENT_STATE_CONNECTED                 = 2
MBUS_CLIENT_STATE_DISCONNECTING             = 3
MBUS_CLIENT_STATE_DISCONNECTED              = 4

try:
    time_func = time.monotonic
except AttributeError:
    time_func = time.time

class MBusClientOptions:
    
    def __init__ (self):
        self.serverProtocol = None
        self.serverAddress = None
        self.serverPort = None
        self.clientName = None
        self.pingInterval = None
        self.pingTimeout = None
        self.pingThreshold = None
    
    def __str__ (self):
        options = {}
        options['server'] = {}
        options['server']['protocol'] = self.serverProtocol
        options['server']['address'] = self.serverAddress
        options['server']['port'] = self.serverPort
        options['client'] = {}
        options['client']['name'] = self.clientName
        options['ping'] = {}
        options['ping']['interval'] = self.pingInterval
        options['ping']['timeout'] = self.pingTimeout
        options['ping']['threshold'] = self.pingThreshold
        return json.dumps(options)

class MBusClientRequest:
    
    def __init__ (self, type, destination, identifier, sequence, payload = None, callback = None, context = None):
        self.type = type
        self.destination = destination
        self.identifier = identifier
        self.sequence = sequence
        self.payload = payload
        self.callback = callback
        self.context = context;

    def __str__ (self):
        request = {}
        request['type'] = self.type
        request['destination'] = self.destination
        request['identifier'] = self.identifier
        request['sequence'] = self.sequence
        request['payload'] = self.payload
        return json.dumps(request)

class MBusClientSubscription:
    
     def __init__ (self, source, event, callback, context = None):
        self._source = source
        self._event = event
        self._callback = callback
        self._context = context

class MBusClient:
    
    def __init__ (self, options = None):
        self.onConnected = None
        self.onSubscribed = None
        
        self._state = MBUS_CLIENT_STATE_INITIAL
        self._socket = None
        self._sequence = MBUS_METHOD_SEQUENCE_START
        
        self._requests = collections.deque()
        self._pendings = collections.deque()
        self._subscriptions = collections.deque()
        self._incoming = ""
        self._outgoing = ""
        
        self._name = None
        self._pingInterval = None
        self._pingTimeout = None
        self._pingThreshold = None
        self._compression = None

        self._pingSendTsms = 0
        self._pongRecvTsms = 0
        self._pingWaitPong = 0
        self._pongMissedCount = 0

        if (options == None):
            self._options = MBusClientOptions()
        else:
            self._options = options
            
        if (self._options.serverProtocol == None):
            self._options.serverProtocol = MBUS_CLIENT_OPTIONS_DEFAULT_SERVER_PROTOCOL
            self._options.serverAddress = MBUS_CLIENT_OPTIONS_DEFAULT_SERVER_ADDRESS
            self._options.serverPort = MBUS_CLIENT_OPTIONS_DEFAULT_SERVER_PORT
        
        if (self._options.clientName == None):
            self._options.clientName = MBUS_CLIENT_OPTIONS_DEFAULT_CLIENT_NAME
        
        if (self._options.pingInterval == None):
            self._options.pingInterval = MBUS_CLIENT_OPTIONS_DEFAULT_PING_INTERVAL

        if (self._options.pingTimeout == None):
            self._options.pingTimeout = MBUS_CLIENT_OPTIONS_DEFAULT_PING_TIMEOUT

        if (self._options.pingThreshold == None):
            self._options.pingThreshold = MBUS_CLIENT_OPTIONS_DEFAULT_PING_THRESHOLD

    def __str__ (self):
        client = {}
        client['name'] = self._name
        client['ping'] = {}
        client['ping']['interval'] = self._pingInterval
        client['ping']['timeout'] = self._pingTimeout
        client['ping']['threshold'] = self._pingThreshold
        client['compression'] = self._compression
        client['options'] = self._options.__str__()
        return json.dumps(client)
    
    def _connect (self):

        if (self._socket != None):
            self._socket.close()
            self._socket = None

        self._sequence = MBUS_METHOD_SEQUENCE_START

        self._requests.clear()
        self._pendings.clear()
        self._incoming = ""
        self._outgoing = ""

        self._name = None
        self._pingInterval = None
        self._pingTimeout = None
        self._pingThreshold = None
        self._compression = None

        self._pingSendTsms = 0
        self._pongRecvTsms = 0
        self._pingWaitPong = 0
        self._pongMissedCount = 0

        if (self._options.serverProtocol.lower() == "tcp".lower()):
            self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            try:
                self._socket.connect((self._options.serverAddress, self._options.serverPort))
            except socket.error as error:
                self._socket.close()
                self._socket = None
                return -1
        else:
            return -1
        
        self._socket.setblocking(0)
        
        options = {}
        options['name'] = self._options.clientName
        options['ping'] = {}
        options['ping']['interval'] = MBUS_CLIENT_OPTIONS_DEFAULT_PING_INTERVAL
        options['ping']['timeout'] = MBUS_CLIENT_OPTIONS_DEFAULT_PING_TIMEOUT
        options['ping']['threshold'] = MBUS_CLIENT_OPTIONS_DEFAULT_PING_THRESHOLD
        options['compression'] = []
        options['compression'].append("none")
        request = MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_CREATE, self._sequence, options)
        self._sequence += 1
        if (self._sequence >= MBUS_METHOD_SEQUENCE_END):
            self._sequence = MBUS_METHOD_SEQUENCE_START
        self._requests.append(request)
        
        return 0
    
    def _pongRecv (self, this, context, source, event, payload):
        self._pingWaitPong = 0
        self._pingMissedCount = 0
        self._pongRecvTsms = time_func()
        
    def _handleResult (self, object):
        pending = None
        for p in self._pendings:
            if p.sequence == object['sequence']:
                pending = p
                break
        if pending == None:
            return -1
        self._pendings.remove(pending)
        if (pending.type == MBUS_METHOD_TYPE_COMMAND and \
            pending.destination == MBUS_SERVER_NAME and \
            pending.identifier == MBUS_SERVER_COMMAND_CREATE):
            self._name = object['payload']['name']
            self._pingInterval = object['payload']['ping']['interval']
            self._pingTimeout = object['payload']['ping']['timeout']
            self._pingThreshold = object['payload']['ping']['threshold']
            self._compression = object['payload']['compression']
            if (self._pingInterval > 0):
                self.subscribe(MBUS_SERVER_NAME, MBUS_SERVER_EVENT_PONG, self._pongRecv, self)
            if (self.onConnected != None):
                self.onConnected(self)
        elif (pending.type == MBUS_METHOD_TYPE_COMMAND and \
            pending.destination == MBUS_SERVER_NAME and \
            pending.identifier == MBUS_SERVER_COMMAND_SUBSCRIBE):
            if (self.onSubscribed != None):
                self.onSubscribed(self, pending.payload['source'], pending.payload['event'])
        else:
            if pending.callback != None:
                pending.callback(self, pending.context, pending.destination, pending.identifier, object['result'], object['payload'])

    def _handleEvent (self, object):
        subscriptions = self._subscriptions.__copy__()
        for c in subscriptions:
            s = 0
            e = 0
            if (c._source == MBUS_METHOD_EVENT_SOURCE_ALL):
                s = 1
            else:
                if (object['source'] == c._source):
                    s = 1
                    
            if (c._event == MBUS_METHOD_EVENT_IDENTIFIER_ALL):
                e = 1
            else:
                if (object['identifier'] == c._event):
                    e = 1
            if (s == 1 and e == 1):
                c._callback(self, c._context, object['source'], object['identifier'], object['payload'])
                break;
        
    def _handleStatus (self, object):
        subscriptions = self._subscriptions.__copy__()
        for c in subscriptions:
            s = 0
            e = 0
            if (object['source'] == c._source):
                s = 1
                    
            if (c._event == MBUS_METHOD_STATUS_IDENTIFIER_ALL):
                e = 1
            else:
                if (object['identifier'] == c._event):
                    e = 1
            if (s == 1 and e == 1):
                c._callback(self, c._context, object['source'], object['identifier'], object['payload'])
                break;

    def name (self):
        return self._name
    
    def pending (self):
        return len(self._pendings)
    
    def connect (self):
        self._state = MBUS_CLIENT_STATE_CONNECTING
    
    def subscribe (self, source, event, callback, context):
        payload = {}
        payload['source'] = source
        payload['event'] = event
        request = MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_SUBSCRIBE, self._sequence, payload)
        self._sequence += 1
        if (self._sequence >= MBUS_METHOD_SEQUENCE_END):
            self._sequence = MBUS_METHOD_SEQUENCE_START
        self._requests.append(request)
        subsription = MBusClientSubscription(source, event, callback, context)
        self._subscriptions.append(subsription)
        return 0
    
    def eventTo (self, destination, event, payload = None):
        request = MBusClientRequest(MBUS_METHOD_TYPE_EVENT, destination, event, self._sequence, payload)
        self._sequence += 1
        if (self._sequence >= MBUS_METHOD_SEQUENCE_END):
            self._sequence = MBUS_METHOD_SEQUENCE_START
        self._requests.append(request)

    def event (self, event, payload = None):
        self.eventTo(MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, event, payload)

    def eventToSync (self, destination, event, payload = None):
        data = {}
        data['destination'] = destination
        data['identifier'] = event
        data['event'] = payload
        request = MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_EVENT, self._sequence, data)
        self._sequence += 1
        if (self._sequence >= MBUS_METHOD_SEQUENCE_END):
            self._sequence = MBUS_METHOD_SEQUENCE_START
        self._requests.append(request)

    def eventSync (self, event, payload = None):
        self.eventToSync(MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, event, payload)

    def command (self, destination, command, payload, callback, context):
        request = MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, destination, command, self._sequence, payload, callback, context)
        self._sequence += 1
        if (self._sequence >= MBUS_METHOD_SEQUENCE_END):
            self._sequence = MBUS_METHOD_SEQUENCE_START
        self._requests.append(request)
        
    def run (self, timeout = MBUS_CLIENT_RUN_TIMEOUT):
        if (self._state == MBUS_CLIENT_STATE_CONNECTING):
            rc = self._connect()
            if (rc != 0):
                return -1
            else:
                self._state = MBUS_CLIENT_STATE_CONNECTED
                return 0
        
        if (self._state == MBUS_CLIENT_STATE_CONNECTED):
            current = time_func()
            if (self._pingInterval > 0):
                if (self._pingSendTsms + self._pingInterval < current):
                    self._pingSendTsms = current
                    self._pongRecvTsms = 0
                    self._pingWaitpong = 1
                    self.eventTo(MBUS_SERVER_NAME, MBUS_SERVER_EVENT_PING)
                if (self._pingWaitpong != 0 and \
                    self._pingSendTsms != 0 and \
                    self._pongRecvTsms == 0 and \
                    self._pingSendTsms + self._pingTimeout < current):
                    self._pingWaitpong = 0
                    self._pongMissedCount += 1
                if (self._pongMissedCount > self._pingThreshold):
                    return -1
            
            while (len(self._requests) > 0):
                request = self._requests.popleft()
                data = request.__str__()
                dlen = struct.pack("!I", len(data))
                self._outgoing += dlen
                self._outgoing += data
                if request.type.lower() != MBUS_METHOD_TYPE_EVENT:
                    self._pendings.append(request) 

            rlist = [ self._socket ]
            wlist = [ ]
            if (len(self._outgoing) > 0):
                wlist = [ self._socket ]
            
            try:
                socklist = select.select(rlist, wlist, [], timeout)
            except TypeError:
                return -1
            except ValueError:
                return -1
            except KeyboardInterrupt:
                raise
            except:
                return -1
            
            if self._socket in socklist[0]:
                dlen = 0
                try:
                    data = self._socket.recv(4096)
                except socket.error as error:
                    if error.errno == EAGAIN:
                        pass
                    return -1
                if (len(data) == 0):
                    return -1
                self._incoming += data
            
            if self._socket in socklist[1]:
                dlen = 0
                try:
                    dlen = self._socket.send(self._outgoing)
                except socket.error as error:
                    if error.errno == EAGAIN:
                        pass
                    return -1
                if (dlen > 0):
                    self._outgoing = self._outgoing[dlen:] 
            
            while len(self._incoming) >= 4:
                dlen, = struct.unpack("!I", str(self._incoming[0:4]))
                if (dlen > len(self._incoming) - 4):
                    break
                slice = self._incoming[4:4 + dlen]
                self._incoming = self._incoming[4 + dlen:]
                object = json.loads(slice)
                if (object['type'].lower() == MBUS_METHOD_TYPE_RESULT.lower()):
                    self._handleResult(object)
                elif (object['type'].lower() == MBUS_METHOD_TYPE_STATUS.lower()):
                    self._handleStatus(object)
                elif (object['type'].lower() == MBUS_METHOD_TYPE_EVENT.lower()):
                    self._handleEvent(object)
            
            return 0
                    
    def loop (self):
        while (True):
            rc = self.run()
            if (rc == -1):
                return -1
