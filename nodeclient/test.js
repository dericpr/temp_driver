
var io = require("socket.io/node_modules/socket.io-client");
var fs = require('fs');

var socket = io("http://www.serverurl:8080");

socket.on('connect', function() {
	console.log("connected\n");
});

var args = process.argv.slice(2);
socket.on('temp_data', function(msg) {
	console.log("Got temp : ["+msg.temp_data+"]");
	if ( args[0] ) {
		console.log("Going to " + args[0]);
		fs.writeFile(args[0],msg.temp_data,function(err) {
			if (err) {
				return console.log(err);
			}
		});
	} else {
		fs.writeFile("current_temp",msg.temp_data,function(err) {
			if (err) {
				return console.log(err);
			}
		});
	}
});

