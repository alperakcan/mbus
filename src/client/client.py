
import errno
import socket

MBUS_METHOD_TYPE_CREATE                     = "org.mbus.method.type.create";
MBUS_METHOD_TYPE_COMMAND                    = "org.mbus.method.type.command";
MBUS_METHOD_TYPE_STATUS                     = "org.mbus.method.type.status";
MBUS_METHOD_TYPE_EVENT                      = "org.mbus.method.type.event";
MBUS_METHOD_TYPE_RESULT                     = "org.mbus.method.type.result";

MBUS_METHOD_SEQUENCE_START                  = 1;
MBUS_METHOD_SEQUENCE_END                    = 9999;

MBUS_METHOD_EVENT_SOURCE_ALL                = "org.mbus.method.event.source.all";

MBUS_METHOD_EVENT_DESTINATION_ALL           = "org.mbus.method.event.destination.all";
MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS   = "org.mbus.method.event.destination.subscribers";

MBUS_METHOD_EVENT_IDENTIFIER_ALL            = "org.mbus.method.event.identifier.all";

MBUS_METHOD_STATUS_IDENTIFIER_ALL           = "org.mbus.method.event.status.all";

MBUS_SERVER_NAME                            = "org.mbus.server";

MBUS_SERVER_COMMAND_CREATE                  = "command.create";
MBUS_SERVER_COMMAND_EVENT                   = "command.event";
MBUS_SERVER_COMMAND_CALL                    = "command.call";
MBUS_SERVER_COMMAND_RESULT                  = "command.result";
MBUS_SERVER_COMMAND_STATUS                  = "command.status";
MBUS_SERVER_COMMAND_CLIENTS                 = "command.clients";
MBUS_SERVER_COMMAND_SUBSCRIBE               = "command.subscribe";
MBUS_SERVER_COMMAND_REGISTER                = "command.register";
MBUS_SERVER_COMMAND_UNSUBSCRIBE             = "command.unsubscribe";
MBUS_SERVER_COMMAND_CLOSE                   = "command.close";

MBUS_SERVER_STATUS_CONNECTED                = "status.connected";
MBUS_SERVER_STATUS_DISCONNECTED             = "status.disconnected";
MBUS_SERVER_STATUS_SUBSCRIBED               = "status.subscribed";
MBUS_SERVER_STATUS_SUBSCRIBER               = "status.subscriber";
MBUS_SERVER_STATUS_UNSUBSCRIBED             = "status.unsubscribed";

MBUS_SERVER_EVENT_CONNECTED                 = "event.connected";
MBUS_SERVER_EVENT_DISCONNECTED              = "event.disconnected";
MBUS_SERVER_EVENT_SUBSCRIBED                = "event.subscribed";
MBUS_SERVER_EVENT_UNSUBSCRIBED              = "event.unsubscribed";

MBUS_SERVER_EVENT_PING                      = "event.ping";
MBUS_SERVER_EVENT_PONG                      = "event.pong";

# mbus client options default values
MBUS_CLIENT_OPTIONS_DEFAULT_SERVER_PROTOCOL = "tcp"
MBUS_CLIENT_OPTIONS_DEFAULT_SERVER_ADDRESS  = "127.0.0.1"
MBUS_CLIENT_OPTIONS_DEFAULT_SERVER_PORT     = 8000

MBUS_CLIENT_OPTIONS_DEFAULT_CLIENT_NAME     = ""

MBUS_CLIENT_OPTIONS_DEFAULT_PING_INTERVAL   = 180000
MBUS_CLIENT_OPTIONS_DEFAULT_PING_TIMEOUT    = 5000
MBUS_CLIENT_OPTIONS_DEFAULT_PING_THRESHOLD  = 2

# mbus client loop default timeout in milliseconds
MBUS_CLIENT_RUN_TIMEOUT     = 250

# mbus client states
MBUS_CLIENT_STATE_INITIAL       = 0
MBUS_CLIENT_STATE_CONNECTING    = 1
MBUS_CLIENT_STATE_CONNECTED     = 2
MBUS_CLIENT_STATE_DISCONNECTING = 3
MBUS_CLIENT_STATE_DISCONNECTED  = 4

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
        return "serverProtocol: {}\n" \
               "serverAddress : {}\n" \
               "serverPort    : {}\n" \
               "clientName    : {}\n" \
               "pingInterval  : {}\n" \
               "pingTimeout   : {}\n" \
               "pingThreshold : {}" \
               .format( \
                        self.serverProtocol, \
                        self.serverAddress, \
                        self.serverPort, \
                        self.clientName, \
                        self.pingInterval, \
                        self.pingTimeout, \
                        self.pingThreshold \
                        )

class MBusClientRequest:
    
    def __init__ (self, type, destination, identifier, sequence, payload = None, callback = None):
        self.type = type
        self.destination = destination
        self.identifier = identifier
        self.sequence = sequence
        self.payload = payload
        self.callback = callback

    def __str__ (self):
        return "type       : {}\n" \
               "destination: {}\n" \
               "identifier : {}\n" \
               "sequence   : {}\n" \
               "payload    : {}\n" \
               "callback   : {}" \
               .format( \
                        self.type, \
                        self.destination, \
                        self.identifier, \
                        self.sequence, \
                        self.payload, \
                        self.callback \
                        )
        
class MBusClient:
    
    def __init__ (self, options = None):
        self._state = MBUS_CLIENT_STATE_INITIAL
        self._socket = None;
        self._sequence = MBUS_METHOD_SEQUENCE_START;
        
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
        return "state: {}\n" \
               "options:\n" \
               "{}" \
               .format( \
                        self._state,
                        self._options
                    )
    
    def connect (self):
        self._state = MBUS_CLIENT_STATE_CONNECTING
    
    def _connect (self):

        if (self._socket != None):
            self._socket.close()
            self._socket = None

        self._sequence = MBUS_METHOD_SEQUENCE_START

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
        request = MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_CREATE, self._sequence)
        print(request)
        
        return 0
    
    def run (self, timeout = MBUS_CLIENT_RUN_TIMEOUT):
        if (self._state == MBUS_CLIENT_STATE_CONNECTING):
            rc = self._connect();
            if (rc != 0):
                return -1
            else:
                self._state = MBUS_CLIENT_STATE_CONNECTED
                return 0
        
        return 0
                    
    def loop (self):
        while (True):
            self.run()
        
options = MBusClientOptions()
print options

client = MBusClient()
print client

client.connect()
client.loop()