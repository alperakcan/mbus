
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

MBUS_SERVER_IDENTIFIER                      = "org.mbus.server"

MBUS_SERVER_COMMAND_CREATE                  = "org.mbus.server.command.create"
MBUS_SERVER_COMMAND_EVENT                   = "org.mbus.server.command.event"
MBUS_SERVER_COMMAND_CALL                    = "org.mbus.server.command.call"
MBUS_SERVER_COMMAND_RESULT                  = "org.mbus.server.command.result"
MBUS_SERVER_COMMAND_STATUS                  = "org.mbus.server.command.status"
MBUS_SERVER_COMMAND_CLIENTS                 = "org.mbus.server.command.clients"
MBUS_SERVER_COMMAND_SUBSCRIBE               = "org.mbus.server.command.subscribe"
MBUS_SERVER_COMMAND_REGISTER                = "org.mbus.server.command.register"
MBUS_SERVER_COMMAND_UNSUBSCRIBE             = "org.mbus.server.command.unsubscribe"
MBUS_SERVER_COMMAND_CLOSE                   = "org.mbus.server.command.close"

MBUS_SERVER_EVENT_CONNECTED                 = "org.mbus.server.event.connected"
MBUS_SERVER_EVENT_DISCONNECTED              = "org.mbus.server.event.disconnected"
MBUS_SERVER_EVENT_SUBSCRIBED                = "org.mbus.server.event.subscribed"
MBUS_SERVER_EVENT_UNSUBSCRIBED              = "org.mbus.server.event.unsubscribed"

MBUS_SERVER_EVENT_PING                      = "org.mbus.server.event.ping"
MBUS_SERVER_EVENT_PONG                      = "org.mbus.server.event.pong"

try:
    mbus_clock_get = time.monotonic
except AttributeError:
    def mbus_clock_get ():
        return time.time() * 1000

def mbus_clock_after (a, b):
    if (float(b - a) < 0):
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
    AtMostOnce         = 0
    AtLeastOnce        = 1
    ExactlyOnce        = 2

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
    
def MBusClientConnectStatusString (status):
    if (status == MBusClientConnectStatus.Success):
        return "success"
    if (status == MBusClientConnectStatus.InternalError):
        return "internal error"
    if (status == MBusClientConnectStatus.InvalidProtocol):
        return "invalid protocol"
    if (status == MBusClientConnectStatus.ConnectionRefused):
        return "connection refused"
    if (status == MBusClientConnectStatus.ServerUnavailable):
        return "server unavailable"
    if (status == MBusClientConnectStatus.Timeout):
        return "timeout"
    if (status == MBusClientConnectStatus.InvalidProtocolVersion):
        return "invalid protocol version"
    if (status == MBusClientConnectStatus.InvalidClientIdentifier):
        return "invalid client identifier"
    if (status == MBusClientConnectStatus.ServerError):
        return "server error"

class MBusClientDisconnectStatus:
    Success          = 0
    InternalError    = 1
    ConnectionClosed = 2

def MBusClientDisconnectStatusString (status):
    if (status == MBusClientDisconnectStatus.Success):
        return "success"
    if (status == MBusClientDisconnectStatus.InternalError):
        return "internal error"
    if (status == MBusClientDisconnectStatus.ConnectionClosed):
        return "connection closed"

class MBusClientPublishStatus:
    Success       = 0
    InternalError = 1
    Timeout       = 2
    Canceled      = 3

def MBusClientPublishStatusString (status):
    if (status == MBusClientPublishStatus.Success):
        return "success"
    if (status == MBusClientPublishStatus.InternalError):
        return "internal error"
    if (status == MBusClientPublishStatus.Timeout):
        return "timeout"
    if (status == MBusClientPublishStatus.Canceled):
        return "canceled"
    
class MBusClientSubscribeStatus:
    Success       = 0
    InternalError = 1
    Timeout       = 2
    Canceled      = 3

def MBusClientSubscribeStatusString (status):
    if (status == MBusClientSubscribeStatus.Success):
        return "success"
    if (status == MBusClientSubscribeStatus.InternalError):
        return "internal error"
    if (status == MBusClientSubscribeStatus.Timeout):
        return "timeout"
    if (status == MBusClientSubscribeStatus.Canceled):
        return "canceled"
    
class MBusClientUnsubscribeStatus:
    Success       = 0
    InternalError = 1
    Timeout       = 2
    Canceled      = 3

def MBusClientUnsubscribeStatusString (status):
    if (status == MBusClientUnsubscribeStatus.Success):
        return "success"
    if (status == MBusClientUnsubscribeStatus.InternalError):
        return "internal error"
    if (status == MBusClientUnsubscribeStatus.Timeout):
        return "timeout"
    if (status == MBusClientUnsubscribeStatus.Canceled):
        return "canceled"
    
class MBusClientRegisterStatus:
    Success       = 0
    InternalError = 1
    Timeout       = 2
    Canceled      = 3

def MBusClientRegisterStatusString (status):
    if (status == MBusClientRegisterStatus.Success):
        return "success"
    if (status == MBusClientRegisterStatus.InternalError):
        return "internal error"
    if (status == MBusClientRegisterStatus.Timeout):
        return "timeout"
    if (status == MBusClientRegisterStatus.Canceled):
        return "canceled"
    
class MBusClientUnregisterStatus:
    Success       = 0
    InternalError = 1
    Timeout       = 2
    Canceled      = 3

def MBusClientUnregisterStatusString (status):
    if (status == MBusClientUnregisterStatus.Success):
        return "success"
    if (status == MBusClientUnregisterStatus.InternalError):
        return "internal error"
    if (status == MBusClientUnregisterStatus.Timeout):
        return "timeout"
    if (status == MBusClientUnregisterStatus.Canceled):
        return "canceled"
    
class MBusClientCommandStatus:
    Success       = 0
    InternalError = 1
    Timeout       = 2
    Canceled      = 3

def MBusClientCommandStatusString (status):
    if (status == MBusClientCommandStatus.Success):
        return "success"
    if (status == MBusClientCommandStatus.InternalError):
        return "internal error"
    if (status == MBusClientCommandStatus.Timeout):
        return "timeout"
    if (status == MBusClientCommandStatus.Canceled):
        return "canceled"
    
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
        
        options["identifier"]       = self.identifier
        options["serverProtocol"]   = self.serverProtocol
        options["serverAddress"]    = self.serverAddress
        options["serverPort"]       = self.serverPort
        options["connectTimeout"]   = self.connectTimeout
        options["connectInterval"]  = self.connectInterval
        options["subscribeTimeout"] = self.subscribeTimeout
        options["registerTimeout"]  = self.registerTimeout
        options["commandTimeout"]   = self.commandTimeout
        options["publishTimeout"]   = self.publishTimeout
        options["pingInterval"]     = self.pingInterval
        options["pingTimeout"]      = self.pingTimeout
        options["pingThreshold"]    = self.pingThreshold

        options["onConnect"]        = self.onConnect
        options["onDisconnect"]     = self.onDisconnect
        options["onMessage"]        = self.onMessage
        options["onRoutine"]        = self.onRoutine
        options["onPublish"]        = self.onPublish
        options["onSubscribe"]      = self.onSubscribe
        options["onUnsubscribe"]    = self.onUnsubscribe
        options["onRegistered"]     = self.onRegistered
        options["onUnregistered"]   = self.onUnregistered
        options["onContext"]        = self.onContext

        return json.dumps(options)

class MBusClientRoutine(object):
    
    def __init__ (self, identifier, callback, context):
        self.identifier = identifier
        self.callback   = callback
        self.context    = context

    def __str__ (self):
        routine = {}
        routine["identifier"] = self.identifier
        routine["callback"]   = self.callback
        routine["context"]    = self.context
        return json.dumps(routine)

class MBusClientSubscription(object):
    
    def __init__ (self, source, identifier, callback, context):
        self.source     = source
        self.identifier = identifier
        self.callback   = callback
        self.context    = context

    def __str__ (self):
        subscription = {}
        subscription["source"]     = self.source
        subscription["identifier"] = self.identifier
        subscription["callback"]   = self.callback
        subscription["context"]    = self.context
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
        self.createdAt   = mbus_clock_get()
    
    def __callback (self, client, context, message, status):
        pass
    
    def getJson (self):
        return json.loads(self.__str__())
    
    def __str__ (self):
        request = {}
        request["type"]        = self.type
        request["destination"] = self.destination
        request["identifier"]  = self.identifier
        request["sequence"]    = self.sequence
        request["payload"]     = self.payload
        request["timeout"]     = self.timeout
        return json.dumps(request)

class MBusClientMessageEvent (object):
    def __init__ (self, payload):
        self.__payload = payload
    
    def getSource (self):
        return self.__payload["source"]

    def getDestination (self):
        return self.__payload["destination"]

    def getIdentifier (self):
        return self.__payload["identifier"]
    
    def getPayload (self):
        return self.__payload["payload"]

class MBusClientMessageCommand (object):
    def __init__ (self, request, response):
        self.__request = request.getJson()
        self.__response = response
    
    def getRequestPayload (self):
        return self.__request["payload"]

    def getResponseResult (self):
        return self.__response["result"]

    def getResponsePayload (self):
        return self.__response["payload"]

class MBusClient (object):
    
    def __notifyPublish (self, request, status):
        if (self.__options.onPublish != None):
            message = MBusClientMessageEvent(request)
            self.__options.onPublish(self, self.__options.onContext, message, status)

    def __notifySubscribe (self, source, event, status):
        if (self.__options.onSubscribe != None):
            self.__options.onSubscribe(self, self.__options.onContext, source, event, status)

    def __notifyUnsubscribe (self, source, event, status):
        if (self.__options.onUnsubscribe != None):
            self.__options.onUnsubscribe(self, self.__options.onContext, source, event, status)

    def __notifyRegistered (self, command, status):
        if (self.__options.onRegistered != None):
            self.__options.onRegistered(self, self.__options.onContext, command, status)

    def __notifyUnregistered (self, command, status):
        if (self.__options.onUnregistered != None):
            self.__options.onUnregistered(self, self.__options.onContext, command, status)

    def __notifyCommand (self, request, response, status):
        if (request.callback != None):
            message = MBusClientMessageCommand(request, response)
            request.callback(self, request.context, message, status)

    def __notifyConnect (self, status):
        if (self.__options.onConnect != None):
            self.__options.onConnect(self, self.__options.onContext, status)

    def __notifyDisonnect (self, status):
        if (self.__options.onDisconnect != None):
            self.__options.onDisconnect(self, self.__options.onContext, status)

    def __reset (self):
        if (self.__socket != None):
            try:
                self.__socket.shutdown(socket.SHUT_WR)
            except socket.error as error:
                if (error.errno == errno.ENOTCONN):
                    pass
            self.__socket.close()
            self.__socket = None
        
        self.__incoming = ""
        self.__outgoing = ""
        
        for request in self.__requests:
            if (request.type == MBUS_METHOD_TYPE_EVENT):
                if (request.destination != MBUS_SERVER_IDENTIFIER and
                    request.identifier != MBUS_SERVER_EVENT_PING):
                    self.__notifyPublish(json.loads(request.__str__()), MBusClientPublishStatus.Canceled)
            elif (request.type == MBUS_METHOD_TYPE_COMMAND):
                if (request.identifier == MBUS_SERVER_COMMAND_EVENT):
                    self.__notifyPublish(request.payload, MBusClientPublishStatus.Canceled)
                elif (request.identifier == MBUS_SERVER_COMMAND_SUBSCRIBE):
                    self.__notifySubscribe(request.payload["source"], request.payload["event"], MBusClientSubscribeStatus.Canceled)
                elif (request.identifier == MBUS_SERVER_COMMAND_UNSUBSCRIBE):
                    self.__notifyUnsubscribe(request.payload["source"], request.payload["event"], MBusClientUnsubscribeStatus.Canceled)
                elif (request.identifier == MBUS_SERVER_COMMAND_REGISTER):
                    self.__notifyRegistered(request.payload["command"], MBusClientRegisterStatus.Canceled)
                elif (request.identifier == MBUS_SERVER_COMMAND_UNREGISTER):
                    self.__notifyUnregistered(request.payload["command"], MBusClientUnregisterStatus.Canceled)
                else:
                    self.__notifyCommand(request, None, MBusClientCommandStatus.Canceled)
        self.__requests.clear()

        for pending in self.__pendings:
            if (pending.type == MBUS_METHOD_TYPE_EVENT):
                if (pending.destination != MBUS_SERVER_IDENTIFIER and
                    pending.identifier != MBUS_SERVER_EVENT_PING):
                    self.__notifyPublish(json.loads(pending.__str__()), MBusClientPublishStatus.Canceled)
            elif (pending.type == MBUS_METHOD_TYPE_COMMAND):
                if (pending.identifier == MBUS_SERVER_COMMAND_EVENT):
                    self.__notifyPublish(pending.payload, MBusClientPublishStatus.Canceled)
                elif (pending.identifier == MBUS_SERVER_COMMAND_SUBSCRIBE):
                    self.__notifySubscribe(pending.payload["source"], pending.payload["event"], MBusClientSubscribeStatus.Canceled)
                elif (pending.identifier == MBUS_SERVER_COMMAND_UNSUBSCRIBE):
                    self.__notifyUnsubscribe(pending.payload["source"], pending.payload["event"], MBusClientUnsubscribeStatus.Canceled)
                elif (pending.identifier == MBUS_SERVER_COMMAND_REGISTER):
                    self.__notifyRegistered(pending.payload["command"], MBusClientRegisterStatus.Canceled)
                elif (pending.identifier == MBUS_SERVER_COMMAND_UNREGISTER):
                    self.__notifyUnregistered(pending.payload["command"], MBusClientUnregisterStatus.Canceled)
                else:
                    self.__notifyCommand(pending, None, MBusClientCommandStatus.Canceled)
        self.__pendings.clear()
        
        for routine in self.__routines:
            self.__notifyUnregistered(routine.identifier, MBusClientUnregisterStatus.Canceled)
        self.__routines.clear()
        
        for subscription in self.__subscriptions:
            self.__notifyUnsubscribe(subscription.source, subscription.identifier, MBusClientUnsubscribeStatus.Canceled)
        self.__subscriptions.clear()
        
        self.__identifier      = None
        self.__pingInterval    = 0
        self.__pingTimeout     = 0
        self.__pingThreshold   = 0
        self.__pingSendTsms    = 0
        self.__pongRecvTsms    = 0
        self.__pingWaitPong    = 0
        self.__pongMissedCount = 0
        self.__sequence        = MBUS_METHOD_SEQUENCE_START
        self.__compression     = None
        self.__socketConnected = 0
    
    def __commandRegisterResponse (self, this, context, message, status):
        raise ValueError("not implemented yet")
    
    def __commandUnregisterResponse (self, this, context, message, status):
        raise ValueError("not implemented yet")
    
    def __commandSubscribeResponse (self, this, context, message, status):
        subscription = context
        if (status != MBusClientCommandStatus.Success):
            if (status == MBusClientCommandStatus.InternalError):
                cstatus = MBusClientSubscribeStatus.InternalError
            elif (status == MBusClientCommandStatus.Timeout):
                cstatus = MBusClientSubscribeStatus.Timeout
            else:
                cstatus = MBusClientSubscribeStatus.InternalError
        elif (message.getResponseResult() == 0):
            cstatus = MBusClientSubscribeStatus.Success
            if (subscription != None):
                self.__subscriptions.append(subscription)
        else:
            cstatus = MBusClientSubscribeStatus.InternalError
        self.__notifySubscribe(message.getRequestPayload()["source"], message.getRequestPayload()["event"], cstatus)
    
    def __commandUnsubscribeResponse (self, this, context, message, status):
        subscription = context
        if (status != MBusClientCommandStatus.Success):
            if (status == MBusClientCommandStatus.InternalError):
                cstatus = MBusClientUnsubscribeStatus.InternalError;
            elif (status == MBusClientCommandStatus.Timeout):
                cstatus = MBusClientUnsubscribeStatus.Timeout;
            else:
                cstatus = MBusClientUnsubscribeStatus.InternalError;
        elif (message.getResponseResult() == 0):
            cstatus = MBusClientUnsubscribeStatus.Success;
            if (subscription != None):
                self.__subscriptions.remove(subscription)
        else:
            cstatus = MBusClientUnsubscribeStatus.InternalError
        self.__notifyUnsubscribe(message.getRequestPayload()["source"], message.getRequestPayload()["event"], cstatus)
    
    def __commandEventResponse (self, this, context, message, status):
        if (status != MBusClientCommandStatus.Success):
            if (status == MBusClientCommandStatus.InternalError):
                cstatus = MBusClientPublishStatus.InternalError
            elif (status == MBusClientCommandStatus.Timeout):
                cstatus = MBusClientPublishStatus.Timeout
            else:
                cstatus = MBusClientPublishStatus.InternalError
        elif (message.getResponseResult() == 0):
            cstatus = MBusClientPublishStatus.Success
        else:
            cstatus = MBusClientPublishStatus.InternalError
        self.__notifyPublish(message.getRequestPayload(), status)
    
    def __commandCreateResponse (self, this, context, message, status):
        if (status != MBusClientCommandStatus.Success):
            if (status == MBusClientCommandStatus.InternalError):
                this.__notifyConnect(MBusClientConnectStatus.ServerError)
            elif (status == MBusClientCommandStatus.Timeout):
                this.__notifyConnect(MBusClientConnectStatus.Timeout)
            else:
                this.__notifyConnect(MBusClientConnectStatus.ServerError)
            this.__reset()
            this.__state = MBusClientState.Disconnected
            return
        if (message.getResponseResult() != 0):
            this.__notifyConnect(MBusClientConnectStatus.ServerError)
            this.__reset()
            this.__state = MBusClientState.Disconnected
            return
        payload = message.getResponsePayload()
        if (payload == None):
            this.__notifyConnect(MBusClientConnectStatus.ServerError)
            this.__reset()
            this.__state = MBusClientState.Disconnected
            return
        self.__identifier = payload["identifier"]
        if (self.__identifier == None):
            this.__notifyConnect(MBusClientConnectStatus.ServerError)
            this.__reset()
            this.__state = MBusClientState.Disconnected
            return
        this.__pingInterval = payload["ping"]["interval"]
        this.__pingTimeout = payload["ping"]["timeout"]
        this.__pingThreshold = payload["ping"]["threshold"]
        this.__compression = payload["compression"]
        this.__state = MBusClientState.Connected
        this.__notifyConnect(MBusClientConnectStatus.Success)

    def __commandCreateRequest (self):
        payload = {}
        if (self.__options.identifier != None):
            payload["identifier"] = self._options.identifier
        if (self.__options.pingInterval > 0):
            payload["ping"] = {}
            payload["ping"]["interval"] = self.__options.pingInterval
            payload["ping"]["timeout"] = self.__options.pingTimeout
            payload["ping"]["threshold"] = self.__options.pingThreshold
        payload["compressions"] = []
        payload["compressions"].append("none")
        rc = self.command(MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_CREATE, payload, self.__commandCreateResponse)
        if (rc != 0):
            return -1
        return 0
    
    def __runConnect (self):
        status = MBusClientConnectStatus.InternalError
        self.__reset()
        if (self.__options.serverProtocol == "tcp"):
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
    
    def __handleResult (self, object):
        pending = None
        sequence = object["sequence"]
        if (sequence == None):
            return -1
        for p in self.__pendings:
            if (p.sequence == sequence):
                pending = p
                break
        if (pending == None):
            return -1
        self.__pendings.remove(pending)
        self.__notifyCommand(pending, object, MBusClientCommandStatus.Success)
        return 0

    def __handleEvent (self, object):
        source = object["source"]
        if (source == None):
            return -1
        identifier = object["identifier"]
        if (identifier == None):
            return -1
        if (source == MBUS_SERVER_IDENTIFIER and
            identifier == MBUS_SERVER_EVENT_PONG):
            self.__pingWaitPong = 0
            self.__pingMissedCount = 0
            self.__pongRecvTsms = mbus_clock_get()
        else:
            callback = self.__options.onMessage
            callbackContext = self.__options.onContext
            for s in self.__subscriptions:
                if ((s.source == MBUS_METHOD_EVENT_SOURCE_ALL or s.source == source) and
                    (s.identifier == MBUS_METHOD_EVENT_IDENTIFIER_ALL or s.identifier == identifier)):
                    if (s.callback != None):
                        callback = s.callback
                        callbackContext = s.context
                    break
            if (callback != None):
                message = MBusClientMessageEvent(object)
                callback(self, callbackContext, message)
    
    def __handleCommand (self, object):
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
        self.__connectTsms     = 0
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
    
    def getOptions (self):
        options = None
        options = self.__options
        return options
    
    def getState (self):
        state = None
        state = self.__state
        return state
    
    def getIdentifier (self):
        identifier = None
        identifier = self.__identifier
        return identifier
    
    def getWakeUpFd (self):
        fd = None
        fd = self.__wakeupRead
        return fd
    
    def getWakeUpFdEvents (self):
        raise ValueError("not implemented yet")
    
    def getConnectionFd (self):
        fd = None
        fd = self.__socket
        return fd
    
    def getConnectionFdEvents (self):
        raise ValueError("not implemented yet")
    
    def hasPending (self):
        rc = 0
        if (len(self.__requests) > 0 or
            len(self.__pendings) > 0 or
            len(self.__incoming) > 0 or
            len(self.__outgoing) > 0):
            rc = 1
        return rc
    
    def connect (self):
        if (self.__state != MBusClientState.Connected):
            self.__state = MBusClientState.Connecting
            self.__wakeUp(MBusClientWakeUpReason.Connect)
    
    def disconnect (self):
        if (self.__state != MBusClientState.Disconnected):
            self.__state = MBusClientState.Disconnecting
            self.__wakeUp(MBusClientWakeUpReason.Disconnect)
    
    def subscribe (self, event, callback = None, context = None, source = None, timeout = None):
        if (self.__state != MBusClientState.Connected):
            raise ValueError("client state is not connected: {}".format(self.__state))
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
        subscription = MBusClientSubscription(source, event, callback, context)
        if (subscription == None):
            raise ValueError("can not create subscription")
        payload = {}
        payload["source"] = source
        payload["event"] = event
        rc = self.command(MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_SUBSCRIBE, payload, self.__commandSubscribeResponse, subscription, timeout)
        if (rc != 0):
            raise ValueError("can not call subscribe command")
        return 0

    def unsubscribe (self, event, source = None, timeout = None):
        if (self.__state != MBusClientState.Connected):
            raise ValueError("client state is not connected: {}".format(self.__state))
        if (source == None):
            source = MBUS_METHOD_EVENT_SOURCE_ALL
        if (event == None):
            raise ValueError("event is invalid")
        subscription = None
        for s in self.__subscriptions:
            if (s.source == source and
                s.identifier == event):
                subscription = s
                break
        if (subscription != None):
            raise ValueError("can not find subscription for source: {}, event: {}".format(source, event))
        payload = {}
        payload["source"] = source
        payload["event"] = event
        rc = self.command(MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_UNSUBSCRIBE, payload, self.__commandUnsubscribeResponse, subscription, timeout)
        if (rc != 0):
            raise ValueError("can not call unsubscribe command")
        return 0

    def publish (self, event, payload = None, qos = None, destination = None, timeout = None):
        if (self.__state != MBusClientState.Connected):
            raise ValueError("client state is not connected: {}".format(self.__state))
        if (event == None):
            raise ValueError("event is invalid")
        if (qos == None):
            qos = MBusClientQoS.AtMostOnce
        if (destination == None):
            destination = MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS
        if (timeout == None or
            timeout <= 0):
            timeout = self.__options.publishTimeout
        if (qos == MBusClientQoS.AtMostOnce):
            request = MBusClientRequest(MBUS_METHOD_TYPE_EVENT, destination, event, self.__sequence, payload, None, None, timeout)
            if (request == None):
                raise ValueError("can not create request")
            self.__sequence += 1
            if (self.__sequence >= MBUS_METHOD_SEQUENCE_END):
                self.__sequence = MBUS_METHOD_SEQUENCE_START
            self.__requests.append(request)
        elif (qos == MBusClientQoS.AtLeastOnce):
            cpayload = {}
            cpayload["destination"] = destination
            cpayload["identifier"] = event
            cpayload["payload"] = payload
            self.command(MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_EVENT, cpayload, self.__commandEventResponse, None, timeout)
        else:
            raise ValueError("qos: {} is invalid".format(qos))
        return 0

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
        return 0

    def getRunTimeout (self):
        current = mbus_clock_get()
        timeout = MBusClientDefaults.RunTimeout
        if (self.__state == MBusClientState.Connecting):
            if (self.__socket == None):
                if (mbus_clock_after(current, self.__connectTsms + self.__options.connectInterval)):
                    timeout = 0
                else:
                    timeout = min(timeout, (self.__connectTsms + self.__options.connectInterval) - (current))
            elif (self.__socketConnected == 0):
                if (mbus_clock_after(current, self.__connectTsms + self.__options.connectTimeout)):
                    timeout = 0
                else:
                    timeout = min(timeout, (self.__connectTsms + self.__options.connectTimeout) - (current))
        elif (self.__state == MBusClientState.Connected):
            if (self.__pingInterval > 0):
                if (mbus_clock_after(current, self.__pingSendTsms + self.__pingInterval)):
                    timeout = 0
                else:
                    timeout = min(timeout, (self.__pingSendTsms + self.__pingInterval) - (current))
            for request in self.__requests:
                if (request.timeout >= 0):
                    if (mbus_clock_after(current, request.createdAt + request.timeout)):
                        timeout = 0
                    else:
                        timeout = min(timeout, (request.createdAt + request.timeout) - (current))
            for request in self.__pendings:
                if (request.timeout >= 0):
                    if (mbus_clock_after(current, request.createdAt + request.timeout)):
                        timeout = 0
                    else:
                        timeout = min(timeout, (request.createdAt + request.timeout) - (current))
        elif (self.__state == MBusClientState.Disconnecting):
            timeout = 0
        elif (self.__state == MBusClientState.Disconnected):
            if (self.__options.connectInterval > 0):
                if (mbus_clock_after(current, self.__connectTsms + self.__options.connectInterval)):
                    timeout = 0
                else:
                    timeout = min(timeout, (self.__connectTsms + self.__options.connectInterval) - (current))
        return timeout
        
    def breakRun (self):
        self.__wakeUp(MBusClientWakeUpReason.Break)
        return 0
        
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

        #print("rlist: {}, wlist: {}, ptimeout: {}".format(rlist, wlist, ptimeout))
        try:
            socklist = select.select(rlist, wlist, [], float(ptimeout) / float(1000))
        except TypeError:
            raise ValueError("select failed")
        except ValueError:
            raise ValueError("select failed")
        except KeyboardInterrupt:
            raise ValueError("select failed")
        except:
            raise ValueError("select failed")

        if (self.__wakeupRead in socklist[0]):
            buffer = os.read(self.__wakeupRead, 4)
            if (len(buffer) != 4):
                raise ValueError("wakeup read failed")
            reason, = struct.unpack("I", buffer)

        if (self.__socket != None):
            if (self.__socket in socklist[0]):
                dlen = 0
                try:
                    data = self.__socket.recv(4096)
                except socket.error as error:
                    if error.errno == errno.EAGAIN:
                        pass
                    if error.errno == errno.EINTR:
                        pass
                    if error.errno == errno.EWOULDBLOCK:
                        pass
                    raise ValueError("recv failed")
                if (len(data) == 0):
                    self.__notifyDisonnect(MBusClientDisconnectStatus.ConnectionClosed)
                    self.__reset()
                    self.__state = MBusClientState.Disconnected
                    return 0
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
                    else:
                        self.__notifyConnect(MBusClientConnectStatus.InternalError)
                        raise ValueError("can not connect to server")
                elif (len(self.__outgoing) > 0):
                    dlen = 0
                    try:
                        dlen = self.__socket.send(self.__outgoing)
                    except socket.error as error:
                        if error.errno == errno.EAGAIN:
                            pass
                        if error.errno == errno.EINTR:
                            pass
                        if error.errno == errno.EWOULDBLOCK:
                            pass
                        raise ValueError("send failed")
                    if (dlen > 0):
                        self.__outgoing = self.__outgoing[dlen:]
        
        current = mbus_clock_get()

        while len(self.__incoming) >= 4:
            dlen, = struct.unpack("!I", str(self.__incoming[0:4]))
            if (dlen > len(self.__incoming) - 4):
                break
            slice = self.__incoming[4:4 + dlen]
            self.__incoming = self.__incoming[4 + dlen:]
            #print("recv: {}".format(slice))
            object = json.loads(slice)
            if (object["type"] == MBUS_METHOD_TYPE_RESULT):
                self.__handleResult(object)
            elif (object["type"] == MBUS_METHOD_TYPE_EVENT):
                self.__handleEvent(object)
            else:
                raise ValueError("unknown type: {}", object["type"])

        if (self.__state == MBusClientState.Connecting and
            self.__socket != None and
            self.__socketConnected == 0):
            if (mbus_clock_after(current, self.__connectTsms + self.__options.connectTimeout)):
                self.__notifyConnect(MBusClientConnectStatus.Timeout)
                self.__reset()
                if (self.__options.connectInterval > 0):
                    self.__state == MBusClientState.Connecting
                else:
                    self.__state == MBusClientState.Disconnected
                return 0

        if (self.__state == MBusClientState.Connected and
            self.__pingInterval > 0):
            if (mbus_clock_after(current, self.__pingSendTsms + self.__pingInterval)):
                self.__pingSendTsms = current
                self.__pongRecvTsms = 0
                self.__pingWaitPong = 1
                rc = self.publish(MBUS_SERVER_EVENT_PING, None, None, MBUS_SERVER_IDENTIFIER, None)
                if (rc != 0):
                    raise ValueError("can not publish ping")
            if (self.__pingWaitPong != 0 and
                self.__pingSendTsms != 0 and
                self.__pongRecvTsms == 0 and
                mbus_clock_after(current, self.__pingSendTsms + self.__pingTimeout)):
                self.__pingWaitPong = 0
                self.__pongMissedCount += 1
            if (self.__pongMissedCount > self.__pingThreshold):
                raise ValueError("missed too many pongs, {} > {}".format(self.__pongMissedCount, self.__pingThreshold))

        for request in self.__requests:
            if (request.timeout < 0 or
                mbus_clock_before(current, request.createdAt + request.timeout)):
                continue
            if (request.type == MBUS_METHOD_TYPE_EVENT):
                if (request.destination != MBUS_SERVER_IDENTIFIER and
                    request.identifier != MBUS_SERVER_EVENT_PING):
                    self.__notifyPublish(json.loads(request.__str__()), MBusClientPublishStatus.Timeout)
            elif (request.type == MBUS_METHOD_TYPE_COMMAND):
                if (request.identifier == MBUS_SERVER_COMMAND_EVENT):
                    self.__notifyPublish(request.payload, MBusClientPublishStatus.Timeout)
                elif (request.identifier == MBUS_SERVER_COMMAND_SUBSCRIBE):
                    self.__notifySubscribe(request.payload["source"], request.payload["event"], MBusClientSubscribeStatus.Timeout)
                elif (request.identifier == MBUS_SERVER_COMMAND_UNSUBSCRIBE):
                    self.__notifyUnsubscribe(request.payload["source"], request.payload["event"], MBusClientUnsubscribeStatus.Timeout)
                elif (request.identifier == MBUS_SERVER_COMMAND_REGISTER):
                    self.__notifyRegistered(request.payload["command"], MBusClientRegisterStatus.Timeout)
                elif (request.identifier == MBUS_SERVER_COMMAND_UNREGISTER):
                    self.__notifyUnregistered(request.payload["command"], MBusClientUnregisterStatus.Timeout)
                else:
                    self.__notifyCommand(request, None, MBusClientCommandStatus.Timeout)

        for request in self.__pendings:
            if (request.timeout < 0 or
                mbus_clock_before(current, request.createdAt + request.timeout)):
                continue
            if (request.type == MBUS_METHOD_TYPE_EVENT):
                if (request.destination != MBUS_SERVER_IDENTIFIER and
                    request.identifier != MBUS_SERVER_EVENT_PING):
                    self.__notifyPublish(json.loads(request.__str__()), MBusClientPublishStatus.Timeout)
            elif (request.type == MBUS_METHOD_TYPE_COMMAND):
                if (request.identifier == MBUS_SERVER_COMMAND_EVENT):
                    self.__notifyPublish(request.payload, MBusClientPublishStatus.Timeout)
                elif (request.identifier == MBUS_SERVER_COMMAND_SUBSCRIBE):
                    self.__notifySubscribe(request.payload["source"], request.payload["event"], MBusClientSubscribeStatus.Timeout)
                elif (request.identifier == MBUS_SERVER_COMMAND_UNSUBSCRIBE):
                    self.__notifyUnsubscribe(request.payload["source"], request.payload["event"], MBusClientUnsubscribeStatus.Timeout)
                elif (request.identifier == MBUS_SERVER_COMMAND_REGISTER):
                    self.__notifyRegistered(request.payload["command"], MBusClientRegisterStatus.Timeout)
                elif (request.identifier == MBUS_SERVER_COMMAND_UNREGISTER):
                    self.__notifyUnregistered(request.payload["command"], MBusClientUnregisterStatus.Timeout)
                else:
                    self.__notifyCommand(request, None, MBusClientCommandStatus.Timeout)

        while (len(self.__requests) > 0):
            request = self.__requests.popleft()
            data = request.__str__()
            dlen = struct.pack("!I", len(data))
            self.__outgoing += dlen
            self.__outgoing += data
            #print("send: {}".format(data))
            if request.type == MBUS_METHOD_TYPE_EVENT:
                if (request.destination != MBUS_SERVER_IDENTIFIER and
                    request.identifier != MBUS_SERVER_EVENT_PING):
                    self.__notifyPublish(json.loads(request.__str__()), MBusClientPublishStatus.Success)
            else:
                self.__pendings.append(request)
         
        return 0
