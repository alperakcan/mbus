
'use strict';

const WebSocket = require('ws');

function MBusClient (name, options) {
	if (this instanceof MBusClient == false) {
		return new MBusClient(name, options);
	}
	this.type = 'MBusClient';
	this._socket = new WebSocket("ws://127.0.0.1:9000", "mbus");
}
