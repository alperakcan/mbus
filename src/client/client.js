
const WebSocket = require('ws');

function MBusClient (name, options) {
	if (this instanceof MBusClient == false) {
		return new MBusClient(name, options);
	}
	
	this.type = 'MBusClient';
	
	this._socket = new WebSocket("ws://127.0.0.1:9000", "mbus");
	
	this._socket.on('open', function open() {
		console.log('open');
	})

	this._socket.on('close', function close(code, message) {
		console.log('close, code: ' + code + ', message: ' + message);
	})

	this._socket.on('message', function message(data, flags) {
		console.log('message' + data + flags);
	})

	this._socket.on('error', function error(error) {
		console.log('error: ' + error);
	})
}

module.exports = MBusClient