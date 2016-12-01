
WebSocket = require('ws');

const MBUS_SERVER_NAME				= 'org.mbus.server';
const MBUS_METHOD_TYPE_COMMAND		= 'org.mbus.method.type.command';
const MBUS_METHOD_TYPE_RESULT		= 'org.mbus.method.type.result';
const MBUS_SERVER_COMMAND_CREATE	= 'command.create';
const MBUS_SERVER_COMMAND_SUBSCRIBE	= "command.subscribe";

const MBUS_METHOD_SEQUENCE_START	= 1;
const MBUS_METHOD_SEQUENCE_END		= 9999;

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
	
	this._name = name;
	this._sequence = MBUS_METHOD_SEQUENCE_START;
	this._socket = null;
	this._requests = Array();
	this._pendings = Array();
	this._callbacks = Array();
	this._incoming = Buffer(0);
	
	this._scope = function (f, scope) {
		return function () {
			return f.apply(scope, arguments);
		};
	};
	
	this._sendString = function (message) {
		if (typeof message !== 'string') {
			return false;
		}
		console.log('sending', message.length, message);
	    var b;
	    b = Buffer(4 + message.length);
	    b.fill(0);
	    b.writeUInt32BE(message.length, 0);
	    b.write(message, 4);
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
			console.log('connected');
			this.onConnected();
		}
	}
	
	this._handleIncoming = function () {
		while (this._incoming.length >= 4) {
			var slice;
			var string;
			var expected;
			expected = this._incoming.readUInt32BE(0);
			if (expected < this._incoming.length - 4) {
				break;
			}
			console.log('expected:', expected);
			slice = this._incoming.slice(4, 4 + expected);
			this._incoming = this._incoming.slice(4 + expected);
			string = slice.toString();
			object = JSON.parse(string)
			console.log('string:', object);
			if (object['type'] == MBUS_METHOD_TYPE_RESULT) {
				this._handleResult(object);
			}
		}
	}
}

MBusClient.prototype.connect = function () {
	this._socket = WebSocket("ws://127.0.0.1:9000", 'mbus');

	this._socket.on('open', this._scope(function open() {
		var request;
		console.log('open');
		request = MBusClientRequest(MBUS_METHOD_TYPE_COMMAND, this._name, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_CREATE, this._sequence, null);
		this._sequence += 1;
		if (this._sequence >= MBUS_METHOD_SEQUENCE_END) {
			this._sequence = MBUS_METHOD_SEQUENCE_START;
		}
		this._requests.push(request);
		this._scheduleRequests();
	}, this))

	this._socket.on('message', this._scope(function message(data, flags) {
		console.log('message:', data, 'flags:', flags);
		this._incoming = Buffer.concat([this._incoming, data]);
		console.log('incoming:', this._incoming.length);
		this._handleIncoming();
	}, this))

	this._socket.on('close', this._scope(function close(code, message) {
		console.log('close, code:', code, ', message:', message);
	}, this))

	this._socket.on('error', this._scope(function error(error) {
		console.log('error:', error);
	}, this))
}

MBusClient.prototype.subscribe = function (source, event, callback) {
	var request;
	var payload;
	payload = {
			source: source,
			event: event,
	};
	console.log('subscribe:', payload);
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
	console.log('callbacks: ', this._callbacks);
}

module.exports = MBusClient