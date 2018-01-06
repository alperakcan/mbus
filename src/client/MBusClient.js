
//WebSocket = require('ws');
//TextEncoder = require('text-encoder-lite');
//module.exports = MBusClient

const MBUS_METHOD_TYPE_COMMAND                    = "org.mbus.method.type.command";
const MBUS_METHOD_TYPE_EVENT                      = "org.mbus.method.type.event";
const MBUS_METHOD_TYPE_RESULT                     = "org.mbus.method.type.result";

const MBUS_METHOD_SEQUENCE_START                  = 1;
const MBUS_METHOD_SEQUENCE_END                    = 9999;

const MBUS_METHOD_EVENT_SOURCE_ALL                = "org.mbus.method.event.source.all";
const MBUS_METHOD_EVENT_DESTINATION_ALL           = "org.mbus.method.event.destination.all";
const MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS   = "org.mbus.method.event.destination.subscribers";
const MBUS_METHOD_EVENT_IDENTIFIER_ALL            = "org.mbus.method.event.identifier.all";

const MBUS_METHOD_TAG_TYPE                        = "org.mbus.method.tag.type";
const MBUS_METHOD_TAG_SOURCE                      = "org.mbus.method.tag.source";
const MBUS_METHOD_TAG_DESTINATION                 = "org.mbus.method.tag.destination";
const MBUS_METHOD_TAG_IDENTIFIER                  = "org.mbus.method.tag.identifier";
const MBUS_METHOD_TAG_SEQUENCE                    = "org.mbus.method.tag.sequence";
const MBUS_METHOD_TAG_TIMEOUT                     = "org.mbus.method.tag.timeout";
const MBUS_METHOD_TAG_PAYLOAD                     = "org.mbus.method.tag.payload";
const MBUS_METHOD_TAG_STATUS                      = "org.mbus.method.tag.status";

const MBUS_SERVER_IDENTIFIER                      = "org.mbus.server";

const MBUS_SERVER_COMMAND_CREATE                  = "command.create";
const MBUS_SERVER_COMMAND_EVENT                   = "command.event";
const MBUS_SERVER_COMMAND_CALL                    = "command.call";
const MBUS_SERVER_COMMAND_RESULT                  = "command.result";
const MBUS_SERVER_COMMAND_STATUS                  = "command.status";
const MBUS_SERVER_COMMAND_CLIENTS                 = "command.clients";
const MBUS_SERVER_COMMAND_SUBSCRIBE               = "command.subscribe";
const MBUS_SERVER_COMMAND_UNSUBSCRIBE             = "command.unsubscribe";
const MBUS_SERVER_COMMAND_REGISTER                = "command.register";
const MBUS_SERVER_COMMAND_UNREGISTER              = "command.unregister";
const MBUS_SERVER_COMMAND_CLOSE                   = "command.close";

const MBUS_SERVER_EVENT_CONNECTED                 = "org.mbus.server.event.connected";
const MBUS_SERVER_EVENT_DISCONNECTED              = "org.mbus.server.event.disconnected";
const MBUS_SERVER_EVENT_SUBSCRIBED                = "org.mbus.server.event.subscribed";
const MBUS_SERVER_EVENT_UNSUBSCRIBED              = "org.mbus.server.event.unsubscribed";

const MBUS_SERVER_EVENT_PING                      = "org.mbus.server.event.ping";
const MBUS_SERVER_EVENT_PONG                      = "org.mbus.server.event.pong";

String.prototype.format = function() {
	a = this;
	for (k in arguments) {
		a = a.replace("{" + k + "}", arguments[k])
	}
	return a
}

function mbus_clock_get () {
	var d = new Date();
	return d.getTime();
}

function mbus_clock_after (a, b) {
    if ((b - a) < 0) {
        return 1;
    } else {
        return 0;
    }
}

function mbus_clock_before (a, b) {
	return mbus_clock_after(b, a);
}

var MBusClientDefaults = Object.freeze({
    Identifier        : null,
    
    ServerWSProtocol  : "ws",
    ServerWSAddress   : "127.0.0.1",
    ServerWSPort      : 9000,
    
    ServerProtocol    : "ws",
    ServerAddress     : "127.0.0.1",
    ServerPort        : 9000,

    RunTimeout        : 1000,
    
    ConnectTimeout    : 30000,
    ConnectInterval   : 0,
    SubscribeTimeout  : 30000,
    RegisterTimeout   : 30000,
    CommandTimeout    : 30000,
    PublishTimeout    : 30000,
    
    PingInterval      : 180000,
    PingTimeout       : 5000,
    PingThreshold     : 2
})

var MBusClientQoS = Object.freeze({
	AtMostOnce  : 0,
    AtLeastOnce : 1,
    ExactlyOnce : 2
})

var MBusClientState = Object.freeze({
    Unknown       : 0,
    Connecting    : 1,
    Connected     : 2,
    Disconnecting : 3,
    Disconnected  : 4
})

var MBusClientConnectStatus = Object.freeze({
    Success                 : 0,
    InternalError           : 1,
    InvalidProtocol         : 2,
    ConnectionRefused       : 3,
    ServerUnavailable       : 4,
    Timeout                 : 5,
    Canceled                : 6,
    InvalidProtocolVersion  : 7,
    InvalidIdentifier       : 8,
    ServerError             : 9
})

function MBusClientConnectStatusString (status) {
    if (status == MBusClientConnectStatus.Success) {
        return "success";
    }
    if (status == MBusClientConnectStatus.InternalError) {
        return "internal error";
    }
    if (status == MBusClientConnectStatus.InvalidProtocol) {
        return "invalid protocol";
    }
    if (status == MBusClientConnectStatus.ConnectionRefused) {
        return "connection refused";
    }
    if (status == MBusClientConnectStatus.ServerUnavailable) {
        return "server unavailable";
    }
    if (status == MBusClientConnectStatus.Timeout) {
        return "connection timeout";
    }
    if (status == MBusClientConnectStatus.Canceled) {
        return "connection canceled";
    }
    if (status == MBusClientConnectStatus.InvalidProtocolVersion) {
    	return "invalid protocol version";
    }
    if (status == MBusClientConnectStatus.InvalidIdentifier) {
        return "invalid identifier";
    }
    if (status == MBusClientConnectStatus.ServerError) {
        return "server error";
    }
    return "unknown";
}

var MBusClientDisconnectStatus = Object.freeze({
    Success          : 0,
    InternalError    : 1,
    ConnectionClosed : 2
})

function MBusClientDisconnectStatusString (status) {
    if (status == MBusClientDisconnectStatus.Success) {
        return "success";
    }
    if (status == MBusClientDisconnectStatus.InternalError) {
        return "internal error";
    }
    if (status == MBusClientDisconnectStatus.ConnectionClosed) {
        return "connection closed";
    }
    return "unknown";
}

var MBusClientPublishStatus = Object.freeze({
    Success       : 0,
    InternalError : 1,
    Timeout       : 2,
    Canceled      : 3
})

function MBusClientPublishStatusString (status) {
    if (status == MBusClientPublishStatus.Success) {
        return "success";
    }
    if (status == MBusClientPublishStatus.InternalError) {
        return "internal error";
    }
    if (status == MBusClientPublishStatus.Timeout) {
        return "timeout";
    }
    if (status == MBusClientPublishStatus.Canceled) {
        return "canceled";
    }
    return "unknown";
}
    
var MBusClientSubscribeStatus = Object.freeze({
    Success       : 0,
    InternalError : 1,
    Timeout       : 2,
    Canceled      : 3
})

function MBusClientSubscribeStatusString (status) {
    if (status == MBusClientSubscribeStatus.Success) {
        return "success";
    }
    if (status == MBusClientSubscribeStatus.InternalError) {
        return "internal error";
    }
    if (status == MBusClientSubscribeStatus.Timeout) {
        return "timeout";
    }
    if (status == MBusClientSubscribeStatus.Canceled) {
        return "canceled";
    }
    return "unknown";
}
    
var MBusClientUnsubscribeStatus = Object.freeze({
    Success       : 0,
    InternalError : 1,
    Timeout       : 2,
    Canceled      : 3
})

function MBusClientUnsubscribeStatusString (status) {
    if (status == MBusClientUnsubscribeStatus.Success) {
        return "success";
    }
    if (status == MBusClientUnsubscribeStatus.InternalError) {
        return "internal error";
    }
    if (status == MBusClientUnsubscribeStatus.Timeout) {
        return "timeout";
    }
    if (status == MBusClientUnsubscribeStatus.Canceled) {
        return "canceled";
    }
    return "unknown";
}
    
var MBusClientRegisterStatus = Object.freeze({
    Success       : 0,
    InternalError : 1,
    Timeout       : 2,
    Canceled      : 3
})

function MBusClientRegisterStatusString (status) {
    if (status == MBusClientRegisterStatus.Success) {
        return "success";
    }
    if (status == MBusClientRegisterStatus.InternalError) {
        return "internal error";
    }
    if (status == MBusClientRegisterStatus.Timeout) {
        return "timeout";
    }
    if (status == MBusClientRegisterStatus.Canceled) {
        return "canceled";
    }
    return "unknown";
}
    
var MBusClientUnregisterStatus = Object.freeze({
    Success       : 0,
    InternalError : 1,
    Timeout       : 2,
    Canceled      : 3
})

function MBusClientUnregisterStatusString (status) {
    if (status == MBusClientUnregisterStatus.Success) {
        return "success";
    }
    if (status == MBusClientUnregisterStatus.InternalError) {
        return "internal error";
    }
    if (status == MBusClientUnregisterStatus.Timeout) {
        return "timeout";
    }
    if (status == MBusClientUnregisterStatus.Canceled) {
        return "canceled";
    }
    return "unknown";
}
    
var MBusClientCommandStatus = Object.freeze({
    Success       : 0,
    InternalError : 1,
    Timeout       : 2,
    Canceled      : 3
})

function MBusClientCommandStatusString (status) {
    if (status == MBusClientCommandStatus.Success) {
        return "success";
    }
    if (status == MBusClientCommandStatus.InternalError) {
        return "internal error";
    }
    if (status == MBusClientCommandStatus.Timeout) {
        return "timeout";
    }
    if (status == MBusClientCommandStatus.Canceled) {
        return "canceled";
    }
    return "unknown";
}

function MBusClientOptions () {
    this.identifier       = null;
    
    this.serverProtocol   = null;
    this.serverAddress    = null;
    this.serverPort       = null;
    
    this.connectTimeout   = null;
    this.connectInterval  = null;
    this.subscribeTimeout = null;
    this.registerTimeout  = null;
    this.commandTimeout   = null;
    this.publishTimeout   = null;
    
    this.pingInterval     = null;
    this.pingTimeout      = null;
    this.pingThreshold    = null;
    
    this.onConnect        = null;
    this.onDisconnect     = null;
    this.onMessage        = null;
    this.onResult         = null;
    this.onRoutine        = null;
    this.onPublish        = null;
    this.onSubscribe      = null;
    this.onUnsubscribe    = null;
    this.onRegistered     = null;
    this.onUnregistered   = null;
    this.onContext        = null;
}

function MBusClientRoutine (identifier, callback, context) {
	this.identifier = identifier;
	this.callback   = callback;
	this.context    = context;
}

function MBusClientSubscription (source, identifier, callback, context) {
	this.source     = source;
	this.identifier = identifier;
	this.callback   = callback;
	this.context    = context;
}

function MBusClientRequest (type, destination, identifier, sequence, payload, callback, context, timeout) {
    this.type        = type;
    this.destination = destination;
    this.identifier  = identifier;
    this.sequence    = sequence;
    this.payload     = payload;
    this.callback    = callback;
    this.context     = context;
    this.timeout     = timeout;
    this.createdAt   = mbus_clock_get();
}

MBusClientRequest.prototype.stringify = function () {
    request = {};
    request[MBUS_METHOD_TAG_TYPE]        = this.type;
    request[MBUS_METHOD_TAG_DESTINATION] = this.destination;
    request[MBUS_METHOD_TAG_IDENTIFIER]  = this.identifier;
    request[MBUS_METHOD_TAG_SEQUENCE]    = this.sequence;
    request[MBUS_METHOD_TAG_PAYLOAD]     = this.payload;
    request[MBUS_METHOD_TAG_TIMEOUT]     = this.timeout;
    return JSON.stringify(request);
}

function MBusClientMessageEvent (request) {
	this.__request = request;
}

MBusClientMessageEvent.prototype.getSource = function () {
	return this.__request[MBUS_METHOD_TAG_SOURCE];
}

MBusClientMessageEvent.prototype.getDestination = function () {
	return this.__request[MBUS_METHOD_TAG_DESTINATION];
}

MBusClientMessageEvent.prototype.getIdentifier = function () {
	return this.__request[MBUS_METHOD_TAG_IDENTIFIER];
}

MBusClientMessageEvent.prototype.getPayload = function () {
	return this.__request[MBUS_METHOD_TAG_PAYLOAD];
}

function MBusClientMessageCommand (request, response) {
	this.__request  = JSON.parse(request.stringify());
	this.__response = response;
}

MBusClientMessageCommand.prototype.getRequestPayload = function () {
	return this.__request[MBUS_METHOD_TAG_PAYLOAD];
}

MBusClientMessageCommand.prototype.getResponseStatus = function () {
	return this.__response[MBUS_METHOD_TAG_STATUS];
}

MBusClientMessageCommand.prototype.getResponsePayload = function () {
	return this.__response[MBUS_METHOD_TAG_PAYLOAD];
}

function MBusClient (options = null) {
	this.__options         = null;
	this.__state           = MBusClientState.Disconnected;
	this.__socket          = null;
	this.__requests        = Array();
	this.__pendings        = Array();
	this.__routines        = Array();
	this.__subscriptions   = Array();
	this.__incoming        = new Uint8Array(0)
	this.__outgoing        = new Uint8Array(0)
	this.__identifier      = null;
	this.__connectTsms     = 0;
	this.__pingInterval    = null;
	this.__pingTimeout     = null;
	this.__pingThreshold   = null;
	this.__pingSendTsms    = null;
	this.__pongRecvTsms    = null;
	this.__pingWaitPong    = null;
	this.__pongMissedCount = null;
	this.__compression     = null;
	this.__socketConnected = null;
	this.__sequence        = null;

	if (options == undefined) {
		this.__options = new MBusClientOptions();
	} else {
		this.__options = Object.assign(new MBusClientOptions(), options);
	}
    
	if (this.__options.identifier == null) {
		this.__options.identifier = MBusClientDefaults.Identifier;
	}
    
	if (this.__options.connectTimeout == null ||
		this.__options.connectTimeout <= 0) {
		this.__options.connectTimeout = MBusClientDefaults.ConnectTimeout;
	}
	if (this.__options.connectInterval == null ||
		this.__options.connectInterval <= 0) {
		this.__options.connectInterval = MBusClientDefaults.ConnectInterval;
	}
	if (this.__options.subscribeTimeout == null ||
		this.__options.subscribeTimeout <= 0) {
		this.__options.subscribeTimeout = MBusClientDefaults.SubscribeTimeout;
	}
	if (this.__options.registerTimeout == null ||
		this.__options.registerTimeout <= 0) {
		this.__options.registerTimeout = MBusClientDefaults.RegisterTimeout;
	}
	if (this.__options.commandTimeout == null ||
		this.__options.commandTimeout <= 0) {
		this.__options.commandTimeout = MBusClientDefaults.CommandTimeout;
	}
	if (this.__options.publishTimeout == null ||
		this.__options.publishTimeout <= 0) {
		this.__options.publishTimeout = MBusClientDefaults.PublishTimeout;
	}

	if (this.__options.pingInterval == null ||
		this.__options.pingInterval == 0) {
		this.__options.pingInterval = MBusClientDefaults.PingInterval;
	}

	if (this.__options.pingTimeout == null ||
		this.__options.pingTimeout == 0) {
		this.__options.pingTimeout = MBusClientDefaults.PingTimeout;
	}

	if (this.__options.pingThreshold == null ||
		this.__options.pingThreshold == 0) {
		this.__options.pingThreshold = MBusClientDefaults.PingThreshold;
	}

	if (this.__options.serverProtocol == null) {
		this.__options.serverProtocol = MBusClientDefaults.ServerProtocol;
	}

	if (this.__options.serverProtocol == MBusClientDefaults.ServerWSProtocol) {
		if (this.__options.serverAddress == null) {
			this.__options.serverAddress = MBusClientDefaults.ServerWSAddress;
		}
		if (this.__options.serverPort == null ||
			this.__options.serverPort <= 0) {
			this.__options.serverPort = MBusClientDefaults.ServerWSPort;
		}
	} else {
		throw "invalid server protocol: {0}".format(this.__options.serverProtocol);
	}

	this.__scope = function (f, scope) {
		return function () {
			return f.apply(scope, arguments);
		}
	}

	this.__connectTime = null;
	this.__pingTimer = setInterval(function __pingTimerCallback(thiz) {
		if (thiz.__state == MBusClientState.Connected &&
			thiz.__pingInterval > 0) {
			current = mbus_clock_get();
			if (mbus_clock_after(current, thiz.__pingSendTsms + thiz.__pingInterval)) {
				thiz.__pingSendTsms = current;
				thiz.__pongRecvTsms = 0;
				thiz.__pingWaitPong = 1;
				rc = thiz.publish(MBUS_SERVER_EVENT_PING, null, null, MBUS_SERVER_IDENTIFIER, null);
				if (rc != 0) {
					throw "can not publish ping";
				}
			}
			if (thiz.__pingWaitPong != 0 &&
				thiz.__pingSendTsms != 0 &&
				thiz.__pongRecvTsms == 0 &&
				mbus_clock_after(current, thiz.__pingSendTsms + thiz.__pingTimeout)) {
				thiz.__pingWaitPong = 0;
				thiz.__pongMissedCount += 1;
			}
			if (thiz.__pongMissedCount > thiz.__pingThreshold) {
				throw "missed too many pongs, {0} > {1}".format(thiz.__pongMissedCount, thiz.__pingThreshold);
			}
		}
	}, 1000, this);

	this.__sendString = function (message) {
		var b;
		var s;
		if (typeof message !== 'string') {
			return false;
		}
		s = new TextEncoder("utf-8").encode(message);
		b = new Uint8Array(4 + s.length);
		b.fill(0);
		b[0] = s.length >> 0x16;
		b[1] = s.length >> 0x10;
		b[2] = s.length >> 0x08;
		b[3] = s.length >> 0x00;
		b.set(s, 4);
		this.__socket.send(b);
		return true;
	}

	this.__scheduleRequests = function () {
		var request;
		while (this.__requests.length > 0) {
			request = this.__requests.shift();
			this.__sendString(request.stringify());
			if (request.type == MBUS_METHOD_TYPE_EVENT) {
				if (request.destination != MBUS_SERVER_IDENTIFIER &&
					request.identifier != MBUS_SERVER_EVENT_PING) {
					this.__notifyPublish(JSON.parse(request.stringify()), MBusClientPublishStatus.Success);
				}
			} else {
				this.__pendings.push(request);
			}
		}
	}
	
    this.__notifyPublish = function (request, status) {
    	if (this.__options.onPublish != null) {
    		message = new MBusClientMessageEvent(request);
    		this.__options.onPublish(this, this.__options.onContext, message, status);
    	}
    }
    
    this.__notifySubscribe = function (source, event, status) {
		if (this.__options.onSubscribe != null) {
			this.__options.onSubscribe(this, this.__options.onContext, source, event, status);
		}
	}

	this.__notifyUnsubscribe = function (source, event, status) {
		if (this.__options.onUnsubscribe != null) {
			this.__options.onUnsubscribe(this, this.__options.onContext, source, event, status);
		}
	}

	this.__notifyCommand = function (request, response, status) {
		callback = this.__options.onResult;
		context = this.__options.onContext
		if (request.callback != null) {
			callback = request.callback;
			context = request.context;
		}
		if (callback != null)
			message = new MBusClientMessageCommand(request, response);
			callback(this, context, message, status);
		}
	}

	this.__notifyConnect = function (status) {
		if (this.__options.onConnect !=  null) {
			this.__options.onConnect(this, this.__options.onContext, status);
		}
	}

	this.__notifyDisonnect = function (status) {
		if (this.__options.onDisconnect != null) {
			this.__options.onDisconnect(this, this.__options.onContext, status);
		}
	}

	this.__reset = function () {
		if (this.__socket != null) {
			this.__socket = null;
		}
		
		this.__incoming        = new Uint8Array(0);
		this.__outgoing        = new Uint8Array(0);
		
		this.__requests        = Array();
		this.__pendings        = Array();
		this.__routines        = Array();
		this.__subscriptions   = Array();
		
		this.__identifier      = null;
		this.__pingInterval    = 0;
		this.__pingTimeout     = 0;
		this.__pingThreshold   = 0;
		this.__pingSendTsms    = 0;
		this.__pongRecvTsms    = 0;
		this.__pingWaitPong    = 0;
		this.__pongMissedCount = 0;
		this.__sequence        = MBUS_METHOD_SEQUENCE_START;
		this.__compression     = null;
		this.__socketConnected = 0;
	}

	this.__commandRegisterResponse = function (thiz, context, message, status) {
	}
	
	this.__commandUnegisterResponse = function (thiz, context, message, status) {
	}
	
	this.__commandSubscribeResponse = function (thiz, context, message, status) {
		subscription = context;
		if (status != MBusClientCommandStatus.Success) {
			if (status == MBusClientCommandStatus.InternalError) {
				cstatus = MBusClientSubscribeStatus.InternalError;
			} else if (status == MBusClientCommandStatus.Timeout) {
				cstatus = MBusClientSubscribeStatus.Timeout;
			} else if (status == MBusClientCommandStatus.Canceled) {
				cstatus = MBusClientSubscribeStatus.Canceled;
			} else {
				cstatus = MBusClientSubscribeStatus.InternalError;
			}
		} else if (message.getResponseStatus() == 0) {
			cstatus = MBusClientSubscribeStatus.Success;
			thiz.__subscriptions.push(subscription);
		} else {
			cstatus = MBusClientSubscribeStatus.InternalError;
		}
		thiz.__notifySubscribe(subscription.source, subscription.identifier, cstatus);
	}

	this.__commandUnsubscribeResponse = function (thiz, context, message, status) {
		subscription = context;
		if (status != MBusClientCommandStatus.Success) {
			if (status == MBusClientCommandStatus.InternalError) {
				cstatus = MBusClientUnsubscribeStatus.InternalError;
			} else if (status == MBusClientCommandStatus.Timeout) {
				cstatus = MBusClientUnsubscribeStatus.Timeout;
			} else if (status == MBusClientCommandStatus.Canceled) {
				cstatus = MBusClientUnsubscribeStatus.Canceled;
			} else {
				cstatus = MBusClientUnsubscribeStatus.InternalError;
			}
		} else if (message.getResponseStatus() == 0) {
			cstatus = MBusClientUnsubscribeStatus.Success;
			thiz.__subscriptions.remove(subscription);
		} else {
			cstatus = MBusClientUnsubscribeStatus.InternalError;
		}
		thiz.__notifyUnsubscribe(subscription.source, subscription.identifier, cstatus);
	}

	this.__commandCreateResponse = function (thiz, context, message, status) {
		if (status != MBusClientCommandStatus.Success) {
			if (status == MBusClientCommandStatus.InternalError) {
				thiz.__notifyConnect(MBusClientConnectStatus.ServerError);
			} else if (status == MBusClientCommandStatus.Timeout) {
				thiz.__notifyConnect(MBusClientConnectStatus.Timeout);
			} else if (status == MBusClientCommandStatus.Canceled) {
				thiz.__notifyConnect(MBusClientConnectStatus.Canceled);
			} else {
				thiz.__notifyConnect(MBusClientConnectStatus.ServerError);
			}
			thiz.__reset();
			thiz.__state = MBusClientState.Disconnected;
			return;
		}
		if (message.getResponseStatus() != 0) {
			thiz.__notifyConnect(MBusClientConnectStatus.ServerError);
			thiz.__reset();
			thiz.__state = MBusClientState.Disconnected;
			return;
		}
		payload = message.getResponsePayload();
		if (payload ==  null) {
			thiz.__notifyConnect(MBusClientConnectStatus.ServerError);
			thiz.__reset();
			thiz.__state = MBusClientState.Disconnected;
			return;
		}
		thiz.__identifier = payload["identifier"];
		if (thiz.__identifier ==  null) { 
			thiz.__notifyConnect(MBusClientConnectStatus.ServerError);
			thiz.__reset();
			thiz.__state = MBusClientState.Disconnected;
			return;
		}
		thiz.__pingInterval = payload["ping"]["interval"];
		thiz.__pingTimeout = payload["ping"]["timeout"];
		thiz.__pingThreshold = payload["ping"]["threshold"];
		thiz.__compression = payload["compression"];
		thiz.__state = MBusClientState.Connected;
		thiz.__notifyConnect(MBusClientConnectStatus.Success);
	}

	this.__handleEvent = function (object) {
		source = object[MBUS_METHOD_TAG_SOURCE];
		if (source == null) {
			return -1;
		}
		identifier = object[MBUS_METHOD_TAG_IDENTIFIER]
		if (identifier == null) {
			return -1;
		}
		if (source == MBUS_SERVER_IDENTIFIER &&
			identifier == MBUS_SERVER_EVENT_PONG) {
			this.__pingWaitPong = 0;
			this.__pingMissedCount = 0;
			this.__pongRecvTsms = mbus_clock_get();
		} else {
			callback = this.__options.onMessage;
			callbackContext = this.__options.onContext
			subscriptions = this.__subscriptions.filter(function (subscription) {
				return ((subscription.source == MBUS_METHOD_EVENT_SOURCE_ALL || subscription.source == source) &&
						(subscription.identifier == MBUS_METHOD_EVENT_IDENTIFIER_ALL || subscription.identifier == identifier));
			});
			if (subscriptions.length > 0) {
				if (subscriptions[0].callback != null) {
					callback = subscriptions[0].callback;
					callbackContext = subscriptions[0].context;
				}
			}
			if (callback != null) {
                message = new MBusClientMessageEvent(object)
                callback(this, callbackContext, message)
			}
		}
	}
	
	this.__handleResult = function (object) {
		var index;
		var requests;
		requests = this.__pendings.filter(function (request) {
			return request.sequence == object[MBUS_METHOD_TAG_SEQUENCE];
		});
		if (requests.length == 0) {
			console.log('can not find request for sequence:', object[MBUS_METHOD_TAG_SEQUENCE]);
			return -1;
		}
		if (requests.length > 1) {
			console.log('too many request with sequence:', object[MBUS_METHOD_TAG_SEQUENCE]);
			return -1;
		}
		index = this.__pendings.indexOf(requests[0]);
		if (index == -1) {
			console.log('can not find request for sequence:', object[MBUS_METHOD_TAG_SEQUENCE]);
			return -1;
		}
		request = this.__pendings[index];
		this.__pendings.splice(index, 1);
		this.__notifyCommand(request, object, MBusClientCommandStatus.Success);
		return 0;
	}

	this.__handleIncoming = function () {
		while (this.__incoming.length >= 4) {
			var slice;
			var string;
			var expected;
			expected  = this.__incoming[0] << 0x18;
			expected |= this.__incoming[1] << 0x10;
			expected |= this.__incoming[2] << 0x08;
			expected |= this.__incoming[3] << 0x00;
			if (expected > this.__incoming.length - 4) {
				break;
			}
			slice = this.__incoming.slice(4, 4 + expected);
			this.__incoming = this.__incoming.slice(4 + expected);
			string = new TextDecoder("utf-8").decode(slice);
			object = JSON.parse(string)
			if (object[MBUS_METHOD_TAG_TYPE] == MBUS_METHOD_TYPE_RESULT) {
				this.__handleResult(object);
			} else if (object[MBUS_METHOD_TAG_TYPE] == MBUS_METHOD_TYPE_EVENT) {
				this.__handleEvent(object);
			} else {
				console.log("unknown type: {}".format(object[MBUS_METHOD_TAG_TYPE]));
			}
		}
	}
}

MBusClient.prototype.command = function (destination, command, payload, callback = null, context = null, timeout = null) {
    if (destination == null) {
        console.log("destination is invalid");
        return -1;
    }
    if (command == null) {
        console.log("command is invalid");
        return -1;
    }
    if (command == MBUS_SERVER_COMMAND_CREATE) {
        if (this.__state != MBusClientState.Connecting) {
            console.log("client state is not connecting: {0}".format(this.__state));
            return -1;
        }
    } else {
        if (this.__state != MBusClientState.Connected) {
            console.log("client state is not connected: {0}".format(this.__state));
            return -1;
        }
    }
    if (timeout == null ||
        timeout <= 0) {
        timeout = this.__options.commandTimeout;
    }
    request = new MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, destination, command, this.__sequence, payload, callback, context, timeout);
    if (request == null) {
        console.log("can not create request");
        return -1;
    }
    this.__sequence += 1
    if (this.__sequence >= MBUS_METHOD_SEQUENCE_END) {
        this.__sequence = MBUS_METHOD_SEQUENCE_START;
    }
    this.__requests.push(request);
    this.__scheduleRequests();
    return 0
}

MBusClient.prototype.subscribe = function (event, callback = null, context = null, source = null, timeout = null) {
	if (this.__state != MBusClientState.Connected) {
		console.log("client state is not connected: {}".format(this.__state));
		return -1;
	}
	if (source == null) {
		source = MBUS_METHOD_EVENT_SOURCE_ALL;
	}
	if (event == null) {
		console.log("event is invalid");
		return -1;
	}
	subscriptions = this.__subscriptions.filter(function (subscription) {
		return subscription.source == source &&
		       subscription.identifier == event;
	});
	if (subscriptions.length != 0) {
    	console.log("subscription already exists");
		return -1;
	}
    if (timeout == null ||
        timeout < 0) {
        timeout = this.__options.subscribeTimeout;
    }
    subscription = new MBusClientSubscription(source, event, callback, context);
    if (subscription == null) {
        console.log("can not create subscription");
		return -1;
    }
    payload = {};
    payload["source"] = source;
    payload["event"] = event;
    rc = this.command(MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_SUBSCRIBE, payload, this.__commandSubscribeResponse, subscription, timeout);
    if (rc != 0) {
        console.log("can not call subscribe command");
		return -1;
    }
    return 0;
}

MBusClient.prototype.unsubscribe = function (event, source = null, timeout = null) {
	if (this.__state != MBusClientState.Connected) {
		console.log("client state is not connected: {}".format(this.__state));
		return -1;
	}
	if (source == null) {
		source = MBUS_METHOD_EVENT_SOURCE_ALL;
	}
	if (event == null) {
		console.log("event is invalid");
		return -1;
	}
	subscriptions = this.__subscription.filter(function (subscription) {
		return subscription.source == source &&
		       subscription.identifier == event;
	});
	if (subscriptions.length != 1) {
    	console.log("can not find subscription");
		return -1;
	}
	subscription = subscriptions[0];
	if (subscription == null) {
		console.log("can not find subscription");
		return -1;
	}
    payload = {};
    payload["source"] = source;
    payload["event"] = event;
    rc = this.command(MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_UNSUBSCRIBE, payload, this.__commandUnsubscribeResponse, subscription, timeout);
    if (rc != 0) {
        console.log("can not call unsubscribe command");
		return -1;
    }
    return 0;
}

MBusClient.prototype.publish  = function (event, payload = null, qos = null, destination = null, timeout = null) {
	if (this.__state != MBusClientState.Connected) {
		console.log("client state is not connected: {0}".format(this.__state));
		return -1;
	}
	if (event == null) {
		console.log("event is invalid");
		return -1;
	}
	if (qos == null) {
		qos = MBusClientQoS.AtMostOnce
	}
	if (destination == null) {
		destination = MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS;
	}
	if (timeout == null ||
		timeout <= 0) {
		timeout = this.__options.publishTimeout
	}
	if (qos == MBusClientQoS.AtMostOnce) {
		request = new MBusClientRequest(MBUS_METHOD_TYPE_EVENT, destination, event, this.__sequence, payload, null, null, timeout);
		if (request == null) {
			console.log("can not create request");
			return -1;
		}
		this.__sequence += 1;
		if (this.__sequence >= MBUS_METHOD_SEQUENCE_END) {
			this.__sequence = MBUS_METHOD_SEQUENCE_START;
		}
		this.__requests.push(request);
		this.__scheduleRequests();
	} else if (qos == MBusClientQoS.AtLeastOnce) {
		cpayload = {};
		cpayload[MBUS_METHOD_TAG_DESTINATION] = destination;
		cpayload[MBUS_METHOD_TAG_IDENTIFIER] = event;
		cpayload[MBUS_METHOD_TAG_PAYLOAD] = payload;
		this.command(MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_EVENT, cpayload, this.__commandEventResponse, null, timeout);
	} else {
		console.log("qos: {0} is invalid".format(qos));
		return -1;
	}
	return 0;
}

MBusClient.prototype.connect = function () {
	var address;
	address = this.__options.serverProtocol + "://" + this.__options.serverAddress + ":" + this.__options.serverPort;

	console.log("conecting: {0}".format(address));
	
	this.__reset();
	this.__socket = new WebSocket(address, 'mbus');
	this.__socket.binaryType = 'arraybuffer';
	this.__state = MBusClientState.Connecting;

	this.__socket.onopen = this.__scope(function open() {
		var request;
		var options;
		payload = {};
		if (this.__options.identifier != null) {
			payload['identifier'] = this.__options.identifier;
		}
        if (this.__options.pingInterval > 0) {
			payload['ping'] = {};
			payload['ping']['interval'] = this.__options.pingInterval;
			payload['ping']['timeout'] = this.__options.pingTimeout;
			payload['ping']['threshold'] = this.__options.pingThreshold;
		}
		payload['compression'] = [];
		payload['compression'].push("none");
        rc = this.command(MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_CREATE, payload, this.__commandCreateResponse);
        if (rc != 0) {
            return -1;
        }
        return 0
	}, this);

	this.__socket.onmessage = this.__scope(function message(event) {
		var b = new Uint8Array(event.data);
		var n = new Uint8Array(this.__incoming.length + b.length);
		n.set(this.__incoming);
		n.set(b, this.__incoming.length);
		this.__incoming = n;
		this.__handleIncoming();
	}, this);
	
	this.__socket.onclose =  this.__scope(function close(event) {
		console.log('close, event:', event.code, event.reason);
		this.__reset();
        if (this.__state == MBusClientState.Connecting) {
            this.__notifyConnect(MBusClientConnectStatus.ConnectionRefused);
        } else if (this.__state == MBusClientState.Connected) {
            this.__notifyDisonnect(MBusClientDisconnectStatus.ConnectionClosed);
        }
        if (this.__options.connectInterval > 0) {
            this.__state = MBusClientState.Connecting;
            if (this.__connectTimer != null) {
            	clearTimeout(this.__connectTimer);
            	this.__connectTimer = null;
            }
        	this.__connectTimer = setTimeout(function __connectTimerCallback(thiz) {
        		thiz.connect();
        	}, this.__options.connectInterval, this);
        } else {
        	this.__state = MBusClientState.Disconnected;
        }
	}, this);

	this.__socket.onerror = this.__scope(function error(event) {
		console.log('error, event:', event);
		this.__reset();
        if (this.__state == MBusClientState.Connecting) {
            this.__notifyConnect(MBusClientConnectStatus.InternalError);
        } else if (this.__state == MBusClientState.Connected) {
            this.__notifyDisonnect(MBusClientDisconnectStatus.InternalError);
        }
    	this.__state = MBusClientState.Disconnected;
	}, this);
}
