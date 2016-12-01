
WebSocket = require('ws');
var inherits = require('inherits')

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
	cb = {
		source: source,
		event: event,
		function: callback,
	}
	return cb;
}

function MBusClientSocket (url) {
	if (this instanceof MBusClientSocket == false) {
		return new MBusClientSocket(url);
	}
	WebSocket.call(this, url, 'mbus');
}
inherits(MBusClientSocket, WebSocket)

MBusClientSocket.prototype.sendString = function (message) {
	if (typeof message !== 'string') {
		return false;
	}
	console.log('sending', message);
    var l;
    l = new Uint32Array(1);
    l[0] = message.length;
    this.send(l);
    this.send(message);
    return true;
}

function MBusClient (name, options) {
	if (this instanceof MBusClient == false) {
		return new MBusClient(name, options);
	}
	
	this.type = 'MBusClient';
	this._callbacks = new Array();
	
	this._socket = new MBusClientSocket("ws://127.0.0.1:9000");
	
	this._socket.on('open', function open() {
		console.log('open');
		this.sendString(name);
	})

	this._socket.on('close', function close(code, message) {
		console.log('close, code:', code, ', message:', message);
	})

	this._socket.on('message', function message(data, flags) {
		console.log('message:', data, 'flags:', flags);
	})

	this._socket.on('error', function error(error) {
		console.log('error:', error);
	})
}

MBusClient.prototype.subscribe = function (source, event, callback) {
	var cb;
	cb = new MBusClientCallback(source, event, callback);
	this._callbacks.push(cb);
	console.log('callbacks: ', this._callbacks);
}

module.exports = MBusClient