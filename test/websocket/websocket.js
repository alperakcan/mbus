
var MBusClient = require('../../src/client/client.js');

var mbc = new MBusClient('test'); 

mbc.subscribe('alper', 'akcan', function (source, event, payload) {
})
