var app = require('express')();
var http = require('http').Server(app);
var io = require('socket.io')(http);
var mysql      = require('mysql');
var connection = mysql.createConnection({
		host     : 'localhost',
		user     : '',
		password : '',
		database : ''
	});

connection.connect();

app.get('/',function(req,res) {
	res.sendFile(__dirname + '/index.html');
});

app.get('/tempdata',function(req,res) {
		connection.query("select temp from set_point", function(err,results) {
			if (err) throw err;
			io.sockets.emit("temp_data",{"temp_data":results[0].temp});		
			res.send("temp_data"+":"+results[0].temp);
	});

});

var res = io
	.of("/tempdata")
	.on('connection',function(socket) {
		connection.query("select temp from set_point", function(err,results) {
			if (err) throw err;
			socket.emit("temp_data",{"temp_data":results[0].temp});		
		});
});

io.on('connection',function(socket) {
	socket.on('disconnect', function() {
	});
});

http.listen(8080,function() {
	console.log('listening on *:8080');
});

