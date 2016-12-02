
//WebSocket = require('ws');
//TextEncoder = require('text-encoder-lite');
//module.exports = MBusClient

const MBUS_SERVER_NAME					= 'org.mbus.server';
const MBUS_METHOD_TYPE_COMMAND			= 'org.mbus.method.type.command';
const MBUS_METHOD_TYPE_RESULT			= 'org.mbus.method.type.result';
const MBUS_METHOD_TYPE_EVENT			= "org.mbus.method.type.event";
const MBUS_METHOD_TYPE_STATUS			= "org.mbus.method.type.status";
const MBUS_SERVER_COMMAND_CREATE		= 'command.create';
const MBUS_SERVER_COMMAND_SUBSCRIBE		= "command.subscribe";

const MBUS_METHOD_EVENT_SOURCE_ALL		= "org.mbus.method.event.source.all";
const MBUS_METHOD_EVENT_IDENTIFIER_ALL	= "org.mbus.method.event.identifier.all";
const MBUS_METHOD_STATUS_IDENTIFIER_ALL	= "org.mbus.method.event.status.all";

const MBUS_METHOD_SEQUENCE_START		= 1;
const MBUS_METHOD_SEQUENCE_END			= 9999;

function MBusClientRequest (type, source, destination, identifier, sequence, payload)
{
	if (this instanceof MBusClientRequest == false) {
		return new MBusClientRequest(type, source, destination, identifier, sequence, payload);
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
	if (typeof payload !== 'object') {
		return null;
	}
	this.type = 'MBusClientRequest';
	this._type = type;
	this._source = source;
	this._destination = destination;
	this._identifier = identifier;
	this._sequence = sequence;
	this._payload = payload;
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

function MBusClientCallback (source, event, callback)
{
	var cb;
	if (this instanceof MBusClientCallback == false) {
		return new MBusClientCallback(source, event, callback);
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
}

function MBusClient (name, options) {
	if (this instanceof MBusClient == false) {
		return new MBusClient(name, options);
	}
	
	this.type = 'MBusClient';
	
	this.onConnected = function () { };
	this.onSubscribed = function (source, event) { };
	
	this._name = name;
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
			this._pendings.push(request);
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
			this.onConnected();
		}
		if (request._type == MBUS_METHOD_TYPE_COMMAND &&
			request._destination == MBUS_SERVER_NAME &&
			request._identifier == MBUS_SERVER_COMMAND_SUBSCRIBE) {
			this.onSubscribed(request._payload['source'], request._payload['event']);
		}
	}

	this._handleEvent = function (object) {
		this._callbacks.forEach(function (callback) {
			if (callback._source !== MBUS_METHOD_EVENT_SOURCE_ALL) {
				if (object['source'] !== callback._source) {
					return;
				}
			}
			if (callback._event !== MBUS_METHOD_EVENT_IDENTIFIER_ALL) {
				if (object['identifier'] !== callback._event) {
					return;
				}
			}
			callback._callback(object['source'], object['identifier'], object['payload']);
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

MBusClient.prototype.connect = function () {
	this._socket = new WebSocket("ws://127.0.0.1:9000", 'mbus');
	this._socket.binaryType = 'arraybuffer';

	this._socket.onopen = this._scope(function open() {
		var request;
		request = MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, this._name, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_CREATE, this._sequence, null);
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

	this._socket.onclose =  this._scope(function close(code, message) {
		console.log('close, code:', code, ', message:', message);
	}, this);

	this._socket.onerror = this._scope(function error(error) {
		console.log('error:', error);
	}, this);
}

MBusClient.prototype.subscribe = function (source, event, callback) {
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
	cb = MBusClientCallback(source, event, callback);
	this._callbacks.push(cb);
}
