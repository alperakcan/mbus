
//WebSocket = require('ws');
//TextEncoder = require('text-encoder-lite');
//module.exports = MBusClient

const MBUS_METHOD_TYPE_CREATE					= "org.mbus.method.type.create";
const MBUS_METHOD_TYPE_COMMAND					= "org.mbus.method.type.command";
const MBUS_METHOD_TYPE_STATUS					= "org.mbus.method.type.status";
const MBUS_METHOD_TYPE_EVENT					= "org.mbus.method.type.event";
const MBUS_METHOD_TYPE_RESULT					= "org.mbus.method.type.result";

const MBUS_METHOD_SEQUENCE_START				= 1;
const MBUS_METHOD_SEQUENCE_END					= 9999;

const MBUS_METHOD_EVENT_SOURCE_ALL				= "org.mbus.method.event.source.all";

const MBUS_METHOD_EVENT_DESTINATION_ALL			= "org.mbus.method.event.destination.all";
const MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS	= "org.mbus.method.event.destination.subscribers";

const MBUS_METHOD_EVENT_IDENTIFIER_ALL			= "org.mbus.method.event.identifier.all";

const MBUS_METHOD_STATUS_IDENTIFIER_ALL			= "org.mbus.method.event.status.all";

const MBUS_SERVER_NAME							= "org.mbus.server";

const MBUS_SERVER_COMMAND_CREATE				= "command.create";
const MBUS_SERVER_COMMAND_EVENT					= "command.event";
const MBUS_SERVER_COMMAND_CALL					= "command.call";
const MBUS_SERVER_COMMAND_RESULT				= "command.result";
const MBUS_SERVER_COMMAND_STATUS				= "command.status";
const MBUS_SERVER_COMMAND_CLIENTS				= "command.clients";
const MBUS_SERVER_COMMAND_SUBSCRIBE				= "command.subscribe";
const MBUS_SERVER_COMMAND_REGISTER				= "command.register";
const MBUS_SERVER_COMMAND_UNSUBSCRIBE			= "command.unsubscribe";
const MBUS_SERVER_COMMAND_CLOSE					= "command.close";

const MBUS_SERVER_STATUS_CONNECTED				= "status.connected";
const MBUS_SERVER_STATUS_DISCONNECTED			= "status.disconnected";
const MBUS_SERVER_STATUS_SUBSCRIBED				= "status.subscribed";
const MBUS_SERVER_STATUS_SUBSCRIBER				= "status.subscriber";
const MBUS_SERVER_STATUS_UNSUBSCRIBED			= "status.unsubscribed";

const MBUS_SERVER_EVENT_CONNECTED				= "event.connected";
const MBUS_SERVER_EVENT_DISCONNECTED			= "event.disconnected";
const MBUS_SERVER_EVENT_SUBSCRIBED				= "event.subscribed";
const MBUS_SERVER_EVENT_UNSUBSCRIBED			= "event.unsubscribed";

const MBUS_SERVER_EVENT_PING					= "event.ping";
const MBUS_SERVER_EVENT_PONG					= "event.pong";

function MBusClientRequest (type, source, destination, identifier, sequence, payload, callback)
{
	if (this instanceof MBusClientRequest == false) {
		return new MBusClientRequest(type, source, destination, identifier, sequence, payload, callback);
	}
	if (typeof type !== 'string') {
		return null;
	}
	if (typeof source !== 'string') {
		return null;
	}
	if (typeof destination !== 'string') {
		return null;
	}
	if (typeof identifier !== 'string') {
		return null;
	}
	if (payload === null) {
		payload = { };
	}
	if (callback == null) {
		callback = function () { };
	}
	if (typeof payload !== 'object') {
		return null;
	}
	if (typeof callback !== 'function') {
		return null;
	}
	this.type = 'MBusClientRequest';
	this._type = type;
	this._source = source;
	this._destination = destination;
	this._identifier = identifier;
	this._sequence = sequence;
	this._payload = payload;
	this._callback = callback;
}

MBusClientRequest.prototype.stringify = function () {
	var request;
	request = {
		type: this._type,
		source: this._source,
		destination: this._destination,
		identifier: this._identifier,
		sequence: this._sequence,
		payload: this._payload,
	};
	return JSON.stringify(request);
}

function MBusClientCallback (source, event, callback, context)
{
	var cb;
	if (this instanceof MBusClientCallback == false) {
		return new MBusClientCallback(source, event, callback, context);
	}
	if (typeof source !== 'string') {
		return null;
	}
	if (typeof event !== 'string') {
		return null;
	}
	if (typeof callback !== 'function') {
		return null;
	}
	this.type = 'MBusClientCallback';
	this._source = source;
	this._event = event;
	this._callback = callback;
	this._context = context;
}

function MBusClient (name = "", options = {} ) {
	if (this instanceof MBusClient == false) {
		return new MBusClient(name, options);
	}
	
	this.type = 'MBusClient';
	
	this.onConnected = function () { };
	this.onSubscribed = function (source, event) { };
	this.onDisconnected = function () { };
	
	this._cname = name;
	this._name = name;
	this._ping = null;
	this._pingWaitPong = 0;
	this._pingMissedCount = 0;
	this._pingTimer = null;
	this._pingCheckTimer = null;
	this._compression = null;
	this._sequence = MBUS_METHOD_SEQUENCE_START;
	this._socket = null;
	this._requests = Array();
	this._pendings = Array();
	this._callbacks = Array();
	this._incoming = new Uint8Array(0);
	
	this._scope = function (f, scope) {
		return function () {
			return f.apply(scope, arguments);
		};
	};
	
	this._sendString = function (message) {
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
		this._socket.send(b);
		return true;
	}
	
	this._scheduleRequests = function () {
		var request;
		while (this._requests.length > 0) {
			request = this._requests.shift();
			this._sendString(request.stringify());
			if (request._type !== MBUS_METHOD_TYPE_EVENT) {
				this._pendings.push(request);
			}
		}
	}

	this._pingCheck = function (mbc) {
		console.log("check");
		mbc._pingWaitPong = 0;
		mbc._pingMissedCount += 1;
		if (mbc._pingMissedCount > mbc._ping['threshold']) {
			console.log("missed too many pongs, ", mbc._pingMissedCount, " > ", mbc._ping['threshold']);
			mbc._socket.close();
		}
	}

	this._pingSend = function (mbc) {
		console.log("ping");
		mbc.eventTo(MBUS_SERVER_NAME, MBUS_SERVER_EVENT_PING, null);
		mbc._pingCheckTimer = setTimeout(mbc._pingCheck, mbc._ping['timeout'], mbc)
	}

	this._pongRecv = function (source, event, payload, mbc) {
		console.log("pong");
		mbc._pingWaitPong = 0;
		mbc._pingMissedCount = 0;
		if (mbc._pingCheckTimer !== null) {
			clearTimeout(mbc._pingCheckTimer);
			mbc._pingCheckTimer = null;
		}
	}
	
	this._handleResult = function (object) {
		var index;
		var requests;
		requests = this._pendings.filter(function (request) {
			return request._sequence == object['sequence'];
		});
		if (requests.length == 0) {
			console.log('can not find request for sequence:', object['sequence']);
			return -1;
		}
		if (requests.length > 1) {
			console.log('too many request with sequence:', object['sequence']);
			return -1;
		}
		index = this._pendings.indexOf(requests[0]);
		if (index == -1) {
			console.log('can not find request for sequence:', object['sequence']);
			return -1;
		}
		request = this._pendings[index];
		this._pendings.splice(index, 1);
		if (request._type == MBUS_METHOD_TYPE_COMMAND &&
		    request._destination == MBUS_SERVER_NAME &&
		    request._identifier == MBUS_SERVER_COMMAND_CREATE) {
		    this._name = object['payload']['name'];
		    this._ping = object['payload']['ping'];
			if (this._pingTimer !== null) {
				clearInterval(this._pingTimer);
			}
			if (this._pingCheckTimer !== null) {
				clearInterval(this._pingCheckTimer);
			}
			this._pingWaitPong = 0;
			this._pingMissedCount = 0;
			this._pingTimer = null;
			this._pingCheckTimer = null;
			if (this._ping !== undefined &&
				this._ping !== null &&
				this._ping['interval'] > 0) {
				this._pingTimer = setInterval(this._pingSend, this._ping['interval'], this);
				this.subscribe(MBUS_SERVER_NAME, MBUS_SERVER_EVENT_PONG, this._pongRecv, this)
			}
			this.onConnected();
		} else if (request._type == MBUS_METHOD_TYPE_COMMAND &&
		    request._destination == MBUS_SERVER_NAME &&
		    request._identifier == MBUS_SERVER_COMMAND_SUBSCRIBE) {
			this.onSubscribed(request._payload['source'], request._payload['event']);
		} else {
			//console.log("type:", request._type);
			//console.log("source:", request._source);
			//console.log("destination:", request._destination);
			//console.log("identifier:", request._identifier);
			//console.log("callback:", request._callback);
			//console.log("request.payload:", request._payload);
			//console.log("return.payload:", object['payload']);
			//console.log("return.result:", object['result']);
			if (request._callback !== null) {
				request._callback(object['result'], object['payload']);
			}
		} 
	}

	this._handleEvent = function (object) {
		this._callbacks.every(function (callback) {
			if (callback._source !== MBUS_METHOD_EVENT_SOURCE_ALL) {
				if (object['source'] !== callback._source) {
					return true;
				}
			}
			if (callback._event !== MBUS_METHOD_EVENT_IDENTIFIER_ALL) {
				if (object['identifier'] !== callback._event) {
					return true;
				}
			}
			callback._callback(object['source'], object['identifier'], object['payload'], callback._context);
			return false;
		})
	}
	
	this._handleStatus = function (object) {
		this._callbacks.forEach(function (callback) {
			if (object['source'] !== callback._source) {
				return;
			}
			if (callback._event !== MBUS_METHOD_STATUS_IDENTIFIER_ALL) {
				if (object['identifier'] !== callback._event) {
					return;
				}
			}
			callback._callback(object['source'], object['identifier'], object['payload']);
		})
	}
	
	this._handleIncoming = function () {
		while (this._incoming.length >= 4) {
			var slice;
			var string;
			var expected;
			expected  = this._incoming[0] << 0x18;
			expected |= this._incoming[1] << 0x10;
			expected |= this._incoming[2] << 0x08;
			expected |= this._incoming[3] << 0x00;
			if (expected > this._incoming.length - 4) {
				break;
			}
			slice = this._incoming.slice(4, 4 + expected);
			this._incoming = this._incoming.slice(4 + expected);
			string = new TextDecoder("utf-8").decode(slice);
			object = JSON.parse(string)
			if (object['type'] == MBUS_METHOD_TYPE_RESULT) {
				this._handleResult(object);
			} else if (object['type'] == MBUS_METHOD_TYPE_EVENT) {
				this._handleEvent(object);
			} else if (object['type'] == MBUS_METHOD_TYPE_STATUS) {
				this._handleStatus(object);
			} else {
				console.log('unknown type:', object['type']);
			}
		}
	}
}

MBusClient.prototype.connect = function (address = "ws://127.0.0.1:9000") {
	this._socket = new WebSocket(address, 'mbus');
	this._socket.binaryType = 'arraybuffer';

	this._socket.onopen = this._scope(function open() {
		var request;
		var options;
		options = {};
		options['ping'] = {};
		options['ping']['interval'] = 180000;
		options['ping']['timeout'] = 5000;
		options['ping']['threshold'] = 2;
		options['compression'] = [];
		options['compression'].push("none");
		if (this._pingTimer !== null) {
			clearInterval(this._pingTimer);
		}
		if (this._pingCheckTimer !== null) {
			clearInterval(this._pingCheckTimer);
		}
		this._ping = null;
		this._pingWaitPong = 0;
		this._pingMissedCount = 0;
		this._pingTimer = null;
		this._pingCheckTimer = null;
		this._compression = null;
		request = MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, this._cname, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_CREATE, this._sequence, options);
		this._sequence += 1;
		if (this._sequence >= MBUS_METHOD_SEQUENCE_END) {
			this._sequence = MBUS_METHOD_SEQUENCE_START;
		}
		this._requests.push(request);
		this._scheduleRequests();
	}, this);

	this._socket.onmessage = this._scope(function message(event) {
		var b = new Uint8Array(event.data);
		var n = new Uint8Array(this._incoming.length + b.length);
		n.set(this._incoming);
		n.set(b, this._incoming.length);
		this._incoming = n;
		this._handleIncoming();
	}, this);

	this._socket.onclose =  this._scope(function close(event) {
		console.log('close, event:', event.code, event.reason);
		if (this._pingTimer !== null) {
			clearInterval(this._pingTimer);
		}
		if (this._pingCheckTimer !== null) {
			clearInterval(this._pingCheckTimer);
		}
		this._ping = null;
		this._pingWaitPong = 0;
		this._pingMissedCount = 0;
		this._pingTimer = null;
		this._pingCheckTimer = null;
		this._compression = null;
		this._sequence = MBUS_METHOD_SEQUENCE_START;
		this._socket = null;
		this._requests = Array();
		this._pendings = Array();
		this._callbacks = Array();
		this._incoming = new Uint8Array(0);
		this.onDisconnected(event.code, event.reason);
	}, this);

	this._socket.onerror = this._scope(function error(event) {
		console.log('error, event:', event);
		if (this._pingTimer !== null) {
			clearInterval(this._pingTimer);
		}
		if (this._pingCheckTimer !== null) {
			clearInterval(this._pingCheckTimer);
		}
		this._ping = null;
		this._pingWaitPong = 0;
		this._pingMissedCount = 0;
		this._pingTimer = null;
		this._pingCheckTimer = null;
		this._compression = null;
		this._sequence = MBUS_METHOD_SEQUENCE_START;
		this._socket = null;
		this._requests = Array();
		this._pendings = Array();
		this._callbacks = Array();
		this._incoming = new Uint8Array(0);
		this.onDisconnected(event.code, event.reason);
	}, this);
}

MBusClient.prototype.subscribe = function (source, event, callback, context) {
	var request;
	var payload;
	payload = {
		source: source,
		event: event,
	};
	request = MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, this._name, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_SUBSCRIBE, this._sequence, payload);
	this._sequence += 1;
	if (this._sequence >= MBUS_METHOD_SEQUENCE_END) {
		this._sequence = MBUS_METHOD_SEQUENCE_START;
	}
	this._requests.push(request);
	this._scheduleRequests();

	var cb;
	cb = MBusClientCallback(source, event, callback, context);
	this._callbacks.push(cb);
}

MBusClient.prototype.event = function (identifier, event) {
	var request;
	var payload;
	if (event == null) {
		event = {};
	}
	payload = {
		destination: MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS,
		identifier: identifier,
		event: event,
	};
	request = MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, this._name, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_EVENT, this._sequence, payload);
	this._sequence += 1;
	if (this._sequence >= MBUS_METHOD_SEQUENCE_END) {
		this._sequence = MBUS_METHOD_SEQUENCE_START;
	}
	this._requests.push(request);
	this._scheduleRequests();
}

MBusClient.prototype.eventTo = function (to, identifier, event) {
	var request;
	var payload;
	if (event == null) {
		event = {};
	}
	payload = {
		destination: to,
		identifier: identifier,
		event: event,
	};
	request = MBusClientRequest(MBUS_METHOD_TYPE_EVENT, this._name, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_EVENT, this._sequence, payload);
	this._sequence += 1;
	if (this._sequence >= MBUS_METHOD_SEQUENCE_END) {
		this._sequence = MBUS_METHOD_SEQUENCE_START;
	}
	this._requests.push(request);
	this._scheduleRequests();
}

MBusClient.prototype.command = function (destination, identifier, command, callback) {
	var request;
	var payload;
	if (command == null) {
		command = {};
	}
	if (callback == null) {
		callback = function () { };
	}
	payload = {
		destination: destination,
		identifier: identifier,
		call: command,
	};
	request = MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, this._name, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_CALL, this._sequence, payload, callback);
	this._sequence += 1;
	if (this._sequence >= MBUS_METHOD_SEQUENCE_END) {
		this._sequence = MBUS_METHOD_SEQUENCE_START;
	}
	this._requests.push(request);
	this._scheduleRequests();
}
