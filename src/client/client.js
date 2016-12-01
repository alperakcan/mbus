
WebSocket = require('ws');
var inherits = require('inherits')

const MBUS_SERVER_NAME				= 'org.mbus.server';
const MBUS_METHOD_TYPE_COMMAND		= 'org.mbus.method.type.command';
const MBUS_SERVER_COMMAND_CREATE	= 'command.create';

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
	console.log('request:', this);
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
	cb = {
		source: source,
		event: event,
		function: callback,
	}
	return cb;
}

function MBusClient (name, options) {
	if (this instanceof MBusClient == false) {
		return new MBusClient(name, options);
	}
	
	this.type = 'MBusClient';
	this._sequence = 1;
	this._callbacks = Array();
	this._incoming = Buffer(0);
	
	this._socket = new WebSocket("ws://127.0.0.1:9000", 'mbus');
	
	var scope = function (f, scope) {
		return function () {
			return f.apply(scope, arguments);
		};
	};
	
	this._sendString = function sendString (message) {
		if (typeof message !== 'string') {
			return false;
		}
		console.log('sending', message);
	    var l;
	    l = Uint32Array(1);
	    l[0] = message.length;
	    this._socket.send(l);
	    this._socket.send(message);
	    return true;
	}
	
	this._socket.on('open', scope(function open() {
		var request;
		console.log('open');
		request = MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, name, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_CREATE, this._sequence, null);
		this._sendString(request.stringify());
		delete request;
	}, this))

	this._socket.on('close', scope(function close(code, message) {
		console.log('close, code:', code, ', message:', message);
	}, this))

	this._socket.on('message', scope(function message(data, flags) {
		console.log('message:', data, 'flags:', flags);
		this._incoming = Buffer.concat([this._incoming, data]);
		console.log('incoming:', this._incoming.length);
	}, this))

	this._socket.on('error', scope(function error(error) {
		console.log('error:', error);
	}, this))
}

MBusClient.prototype.subscribe = function (source, event, callback) {
	var cb;
	cb = new MBusClientCallback(source, event, callback);
	this._callbacks.push(cb);
	console.log('callbacks: ', this._callbacks);
}

module.exports = MBusClient