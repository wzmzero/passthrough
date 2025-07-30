-- SQLite

UPDATE server_config SET listen_port = 8080 WHERE listen_port = 9090;

UPDATE client_config SET serial_port = '/dev/ttyS0' WHERE protocol = 'serial'; 

UPDATE client_config SET server_ip = '127.0.0.1'  WHERE server_ip = '192.168.239.1';
