
var WebSocket = require('ws');


var ws = new WebSocket("ws://127.0.0.1:9000", "mbus"); 

ws.on('open', function open() {
	console.log('open');
})

ws.on('close', function close(code, message) {
	console.log('close, code: ' + code + ', message: ' + message);
})

ws.on('message', function message(data, flags) {
	console.log('message' + data + flags);
})

ws.on('error', function error(error) {
	console.log('error: ' + error);
})
