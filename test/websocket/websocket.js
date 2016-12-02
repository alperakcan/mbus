
var MBusClient = require('../../src/client/client.js');

var mbc = new MBusClient('test'); 

mbc.onConnected = function () {
	console.log('connected');
	mbc.subscribe("org.mbus.server", "org.mbus.method.event.status.all", function (source, event, payload) {
		console.log(source + '.' + event, JSON.stringify(payload, null, '\t'));
	})
	mbc.subscribe("org.mbus.method.event.source.all", "org.mbus.method.event.identifier.all", function (source, event, payload) {
		console.log(source + '.' + event, JSON.stringify(payload, null, '\t'));
	})
} 
	
mbc.connect();
