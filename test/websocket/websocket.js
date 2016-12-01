
var MBusClient = require('../../src/client/client.js');

var mbc = new MBusClient('test'); 

mbc.onConnected = function () {
	console.log('connected');
	mbc.subscribe('alper', 'akcan', function (source, event, payload) {
	})
} 
	
mbc.connect();
