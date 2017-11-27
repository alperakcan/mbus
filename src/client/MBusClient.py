
import os
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
    mbus_clock_get = time.monotonic
except AttributeError:
    def mbus_clock_get ():
        return time.time() * 1000

def mbus_clock_after (a, b):
    if (long(b - a) < 0):
        return 1
    else:
        return 0
def mbus_clock_before (a, b):
    return mbus_clock_after(b, a)

class MBusClientDefaults:
    ClientIdentifier = None
    
    ServerTCPProtocol = "tcp"
    ServerTCPAddress  = "127.0.0.1"
    ServerTCPPort     = 8000
    
    ServerProtocol    = ServerTCPProtocol
    ServerAddress     = ServerTCPAddress
    ServerPort        = ServerTCPPort

    RunTimeout        = 1000
    
    ConnectTimeout    = 30000
    ConnectInterval   = 0
    SubscribeTimeout  = 30000
    RegisterTimeout   = 30000
    CommandTimeout    = 30000
    PublishTimeout    = 30000
    
    PingInterval      = 180000
    PingTimeout       = 5000
    PingThreshold     = 2

class MBusClientQoS:
    Async              = 0
    SyncSender         = 1
    SyncReceiver       = 2
    SyncSenderReceiver = 3

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

class MBusClientWakeUpReason:
    Break      = 0
    Connect    = 1
    Disconnect = 2

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
        self.identifier = identifier
        self.callback   = callback
        self.context    = context

    def __str__ (self):
        routine = {}
        routine['identifier'] = self.identifier
        routine['callback']   = self.callback
        routine['context']    = self.context
        return json.dumps(routine)

class MBusClientSubscription(object):
    
    def __init__ (self, source, identifier, callback, context):
        self.source     = source
        self.identifier = identifier
        self.callback   = callback
        self.context    = context

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
        raise ValueError("not implemented yet")
    
    def __commandUnregisterResponse (self, this, context, message, status):
        raise ValueError("not implemented yet")
    
    def __commandSubscribeResponse (self, this, context, message, status):
        raise ValueError("not implemented yet")
    
    def __commandUnsubscribeResponse (self, this, context, message, status):
        raise ValueError("not implemented yet")
    
    def __commandEventResponse (self, this, context, message, status):
        raise ValueError("not implemented yet")
    
    def __commandCreateResponse (self, this, context, message, status):
        raise ValueError("not implemented yet")
    
    def __commandCreateRequest (self):
        payload = {}
        if (self.__options.identifier != None):
            payload['identifier'] = self._options.identifier
        if (self.__options.pingInterval > 0):
            payload['ping'] = {}
            payload['ping']['interval'] = self.__options.pingInterval
            payload['ping']['timeout'] = self.__options.pingTimeout
            payload['ping']['threshold'] = self.__options.pingThreshold
        payload['compressions'] = []
        payload['compressions'].append("none")
        rc = self.command(MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_CREATE, payload, self.__commandCreateResponse)
        if (rc != 0):
            return -1
        return 0
    
    def __runConnect (self):
        status = MBusClientConnectStatus.InternalError
        self.__reset()
        if (self.__options.serverProtocol.lower() == "tcp".lower()):
            self.__socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.__socket.setblocking(0)
            try:
                self.__socket.connect((self.__options.serverAddress, self.__options.serverPort))
                self.__socketConnected = 1
                status = MBusClientConnectStatus.Success
            except socket.error as error:
                if (error.errno == errno.EINPROGRESS):
                    status = MBusClientConnectStatus.Success
                elif (error.errno == errno.ECONNREFUSED):
                    status = MBusClientConnectStatus.ConnectionRefused
                elif (error.errno == errno.ENOENT):
                    status = MBusClientConnectStatus.ServerUnavailable
                else:
                    status = MBusClientConnectStatus.InternalError
        else:
            status = MBusClientConnectStatus.InvalidProtocol
        
        if (status == MBusClientConnectStatus.Success):
            if (self.__socketConnected == 1):
                self.__commandCreateRequest()
            return 0
        elif (status == MBusClientConnectStatus.ConnectionRefused or
              status == MBusClientConnectStatus.ServerUnavailable):
            self.__notifyConnect(status)
            self.__reset()
            if (self.__options.connectInterval > 0):
                self.__state = MBusClientState.Connecting
            else:
                self.__state = MBusClientState.Disconnected
            return 0
        else:
            self.__notifyConnect(status)
            self.__reset()
            return -1
    
    def __handleResult (self, json):
        pending = None
        sequence = json["sequence"]
        if (sequence == None):
            return -1
        for p in self.__pendings:
            if (p.sequence == sequence):
                pending = p
                break
        if (pending == None):
            return -1
        self.__pendings.remove(pending)
        if (pending.callback != None):
            raise ValueError("not implemented yet")
        return 0

    def __handleEvent (self, json):
        raise ValueError("not implemented yet")
    
    def __handleCommand (self, json):
        raise ValueError("not implemented yet")
    
    def __wakeUp (self, reason):
        buffer = struct.pack("I", reason)
        os.write(self.__wakeupWrite, buffer)
    
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
        self.__wakeupRead      = None
        self.__wakeupWrite     = None
        self.__wakeupRead, self.__wakeupWrite = os.pipe()

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
            self.__options.publishTimeout = MBusClientDefaults.PublishTimeout

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
        pass
    
    def unlock (self):
        pass
    
    def getState (self):
        state = None
        state = self.__state
        return state
    
    def getIdentifier (self):
        identifier = None
        identifier = self.__identifier
        return identifier
    
    def getWakeUpFd (self):
        raise ValueError("not implemented yet")
    
    def getWakeUpFdEvents (self):
        raise ValueError("not implemented yet")
    
    def getConnectionFd (self):
        raise ValueError("not implemented yet")
    
    def getConnectionFdEvents (self):
        raise ValueError("not implemented yet")
    
    def hasPending (self):
        raise ValueError("not implemented yet")
    
    def connect (self):
        if (self.__state != MBusClientState.Connected):
            self.__state = MBusClientState.Connecting
            self.__wakeUp(MBusClientWakeUpReason.Connect)
    
    def disconnect (self):
        if (self.__state != MBusClientState.Disconnected):
            self.__state = MBusClientState.Disconnecting
            self.__wakeUp(MBusClientWakeUpReason.Disconnect)
    
    def subscribe (self, event, source = None, callback = None, context = None, timeout = None):
        if (source == None):
            source = MBUS_METHOD_EVENT_SOURCE_ALL
        if (event == None):
            raise ValueError("event is invalid")
        if (callback != None):
            for subscription in self.__subscriptions:
                if (subscription.source == source and
                    subscription.identifier == event):
                    raise ValueError("subscription already exists")
        if (timeout == None or
            timeout < 0):
            timeout = self.__options.subscribeTimeout
        raise ValueError("not implemented yet")

    def unsubscribe (self, event, source = None, timeout = None):
        raise ValueError("not implemented yet")
    
    def publish (self, event, payload = None, qos = None, destination = None, timeout = None):
        raise ValueError("not implemented yet")

    def register (self, command, callback, context, timeout):
        raise ValueError("not implemented yet")
    
    def unregister (self, command):
        raise ValueError("not implemented yet")
    
    def command (self, destination, command, payload, callback, context = None, timeout = None):
        if (destination == None):
            raise ValueError("destination is invalid")
        if (command == None):
            raise ValueError("command is invalid")
        if (command == MBUS_SERVER_COMMAND_CREATE):
            if (self.__state != MBusClientState.Connecting):
                raise ValueError("client state is not connecting: {}".format(self.__state))
        else:
            if (self.__state != MBusClientState.Connected):
                raise ValueError("client state is not connected: {}".format(self.__state))
        if (timeout == None or
            timeout <= 0):
            timeout = self.__options.commandTimeout
        request = MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, destination, command, self.__sequence, payload, callback, context, timeout)
        if (request == None):
            raise ValueError("can not create request")
        self.__sequence += 1
        if (self.__sequence >= MBUS_METHOD_SEQUENCE_END):
            self.__sequence = MBUS_METHOD_SEQUENCE_START
        self.__requests.append(request)

    def getRunTimeout (self):
        current = mbus_clock_get()
        timeout = MBusClientDefaults.RunTimeout
        if (self.__state == MBusClientState.Connecting):
            if (self.__socket == None):
                timeout = min(timeout, self.__options.connectInterval);
            elif (self.__socketConnected == 0):
                timeout = min(timeout, self.__options.connectTimeout);
        elif (self.__state == MBusClientState.Connected):
            if (self.__pingInterval > 0):
                if (mbus_clock_after(current, self.__pingSendTsms + self.__pingInterval)):
                    timeout = 0;
                else:
                    timeout = min(timeout, (self.__pingSendTsms + self.__pingInterval) - (current));
            for request in self.__requests:
                if (request.timeout >= 0):
                    if (mbus_clock_after(current, request_get_created_at(request) + request.timeout)):
                        timeout = 0;
                    else:
                        timeout = min(timeout, (long) ((request_get_created_at(request) + request.timeout) - (current)));
            for request in self.__pendings:
                if (request.timeout >= 0):
                    if (mbus_clock_after(current, request_get_created_at(request) + request.timeout)):
                        timeout = 0;
                    else:
                        timeout = min(timeout, (long) ((request_get_created_at(request) + request.timeout) - (current)));
        elif (self.__state == MBusClientState.Disconnecting):
            timeout = 0;
        elif (self.__state == MBusClientState.Disconnected):
            if (self.__options.connect_interval > 0):
                if (mbus_clock_after(current, self.__connectTsms + self.__options.connect_interval)):
                    timeout = 0;
                else:
                    timeout = min(timeout, (self.__connectTsms + self.__options.connectInterval) - (current));
        return timeout;
        
    def breakRun (self):
        raise ValueError("not implemented yet")
        
    def run (self, timeout = -1):
        if (self.__state == MBusClientState.Connecting):
            if (self.__socket == None):
                current = mbus_clock_get()
                if (self.__options.connectInterval <= 0 or
                    mbus_clock_after(current, self.__connectTsms + self.__options.connectInterval)):
                    self.__connectTsms = mbus_clock_get()
                    rc = self.__runConnect()
                    if (rc != 0):
                        raise ValueError("can not connect client")
        elif (self.__state == MBusClientState.Connected):
            pass
        elif (self.__state == MBusClientState.Disconnecting):
            self.__reset()
            self.__notifyDisonnect(MBusClientDisconnectStatus.Success)
            self.__state = MBusClientState.Disconnected
            return 0
        elif (self.__state == MBusClientState.Disconnected):
            if (self.__options.connectInterval > 0):
                self.__state = MBusClientState.Connecting
                return 0
        else:
            raise ValueError("client state: {} is invalid".format(self.__state))

        rlist = [ self.__wakeupRead ]
        wlist = [ ]
        if (self.__socket != None):
            if (self.__state == MBusClientState.Connecting and
                self.__socketConnected == 0):
                wlist.append(self.__socket)
            else:
                rlist.append(self.__socket)
                if (len(self.__outgoing) > 0):
                    wlist.append(self.__socket)
        
        ptimeout = self.getRunTimeout()
        if (ptimeout < 0 or
            timeout < 0):
            ptimeout = max(ptimeout, timeout)
        else:
            ptimeout = min(ptimeout, timeout)

        print("rlist: {}, wlist: {}, ptimeout: {}".format(rlist, wlist, ptimeout))
        try:
            socklist = select.select(rlist, wlist, [], ptimeout / 1000)
        except TypeError:
            raise ValueError("select failed");
        except ValueError:
            raise ValueError("select failed");
        except KeyboardInterrupt:
            raise ValueError("select failed");
        except:
            raise ValueError("select failed");

        if (self.__wakeupRead in socklist[0]):
            buffer = os.read(self.__wakeupRead, 4);
            if (len(buffer) != 4):
                raise ValueError("wakeup read failed");
            reason, = struct.unpack("I", buffer);

        if (self.__socket != None):
            if (self.__socket in socklist[0]):
                dlen = 0
                try:
                    data = self.__socket.recv(4096)
                except socket.error as error:
                    if error.errno == EAGAIN:
                        pass
                    raise ValueError("recv failed");
                if (len(data) == 0):
                    raise ValueError("recv failed");
                self.__incoming += data
            
            if (self.__socket in socklist[1]):
                if (self.__state == MBusClientState.Connecting and
                    self.__socketConnected == 0):
                    rc = self.__socket.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
                    if (rc == 0):
                        self.__socketConnected = 1
                        self.__commandCreateRequest()
                    elif (rc == errno.ECONNREFUSED):
                        self.__notifyConnect(MBusClientConnectStatus.ConnectionRefused)
                        self.__reset()
                        if (self.__options.connectInterval > 0):
                            self.__state = MBusClientState.Connecting
                        else:
                            self.__state = MBusClientState.Disconnected
                        return 0
                elif (len(self.__outgoing) > 0):
                    dlen = 0
                    try:
                        dlen = self.__socket.send(self.__outgoing)
                    except socket.error as error:
                        if error.errno == errno.EAGAIN:
                            pass
                        raise ValueError("send failed");
                    if (dlen > 0):
                        self.__outgoing = self.__outgoing[dlen:]
        
        while len(self.__incoming) >= 4:
            dlen, = struct.unpack("!I", str(self.__incoming[0:4]))
            if (dlen > len(self.__incoming) - 4):
                break
            slice = self.__incoming[4:4 + dlen]
            self.__incoming = self.__incoming[4 + dlen:]
            print("recv: {}".format(slice))
            object = json.loads(slice)
            if (object['type'].lower() == MBUS_METHOD_TYPE_RESULT.lower()):
                self.__handleResult(object)
            else:
                raise ValueError("unknown type: {}", object['type']);

        while (len(self.__requests) > 0):
            request = self.__requests.popleft()
            data = request.__str__()
            dlen = struct.pack("!I", len(data))
            self.__outgoing += dlen
            self.__outgoing += data
            print("send: {}".format(data))
            if request.type.lower() == MBUS_METHOD_TYPE_EVENT:
                pass
            else:
                self.__pendings.append(request)
         
        return 0
