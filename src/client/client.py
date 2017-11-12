
class MBUSClientOptions:
    
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

class MBusClient:
    
    def __init__ (self, options = None):
        if (options == None):
            self.options = MBUSClientOptions()
        else:
            self.options = options

    def __str__ (self):
        return "options:\n" \
               "{}" \
               .format( \
                         self.options
                         )
        
options = MBUSClientOptions()
client = MBusClient()

print client
#print options