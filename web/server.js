var express = require('express');
var app = express();
var server = require('http').Server(app);
var io = require('socket.io')(server);
var mqtt = require('mqtt');
var mysql = require('mysql');
var data_humedad = [0,0,0,0,0,0,0,0,0,0,0,0,0]
var data_temperatura = [0,0,0,0,0,0,0,0,0,0,0,0,0]
var data_luz = [0,0,0,0,0,0,0,0,0,0,1,1,1]
var dateFormat = require('dateformat');
var fs = require('fs');
var client = mqtt.connect("mqtt://citec.ec");
server.listen(8888);

app.use(express.static('public'));

client.on('connect', function () {
  client.subscribe('humedad');
  client.subscribe('temperatura');
  client.subscribe('luz');
});

client.on('message', function (topic, message) {
  // message is Buffer
  if(topic=="humedad"){
          node = parseInt("001");
          sensor = parseInt("001");
          value = parseFloat(message.toString());
          data_humedad.shift();
          //data.push(parseInt(parseFloat(message)*10));
          data_humedad.push(parseFloat(message));
          io.sockets.emit('pushdata_humedad',message.toString());
          fs.appendFile('/var/www/html/data/humedad.csv', dateFormat(new Date(),"dd/mm/yyyy hh:MM:ss")+","+message.toString()+"\r\n", function (err) {
          connection.query("INSERT INTO valor (valor,sensor_id, node_id) VALUES ('"+value+"','"+sensor+"','"+node+"');", function(err, rows, fields) {});
  });
        }
        else if(topic == "temperatura"){
                data_temperatura.shift();
                //data.push(parseInt(parseFloat(message)*10));
                data_temperatura.push(parseFloat(message));
                io.sockets.emit('pushdata_temperatura',message.toString());
fs.appendFile('/var/www/html/data/temperatura.csv', dateFormat(new Date(),"dd/mm/yyyy hh:MM:ss")+","+message.toString()+"\r\n", function (err) {
  });

        }
        else if(topic == "luz"){
                data_luz.shift();
                //data.push(parseInt(parseFloat(message)*10));
                data_luz.push(parseFloat(message));
                io.sockets.emit('pushdata_luz',message.toString());
fs.appendFile('/var/www/html/data/luz.csv', dateFormat(new Date(),"dd/mm/yyyy hh:MM:ss")+","+message.toString()+"\r\n", function (err) {
  });

        }

});

app.get('/', function(req, res) {
    res.sendFile(__dirname + '/index.html');
});


io.on('connection', function(socket) {
  io.sockets.emit('init_temperatura', data_temperatura);
  io.sockets.emit('init_humedad', data_humedad);
  io.sockets.emit('init_luz', data_luz);
});



var connection = mysql.createConnection({
      host     : 'localhost',
      user     : 'root',
      password : 'cesar1234',
      database : 'egrow'
});

connection.connect(function(err){
    if(!err) {
            console.log("Database is connected ... ");
    } else {
            console.log("Error connecting database ... ");
    }
});

io.on('disconnect', function(socket){
  client.end();
});
