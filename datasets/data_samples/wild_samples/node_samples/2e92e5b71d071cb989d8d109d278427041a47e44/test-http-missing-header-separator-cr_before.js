'use strict';

const common = require('../common');
const assert = require('assert');

const http = require('http');
const net = require('net');

const msg = [
  'GET / HTTP/1.1',
  'Host: localhost',
  'Dummy: x\nContent-Length: 23',
  '',
  'GET / HTTP/1.1',
  'Dummy: GET /admin HTTP/1.1',
  'Host: localhost',
  '',
  '',
].join('\r\n');

const server = http.createServer(common.mustNotCall());

server.listen(0, common.mustSucceed(() => {
  const client = net.connect(server.address().port, 'localhost');

  let response = '';

  client.on('data', common.mustCall((chunk) => {
    response += chunk;
  }));

  client.setEncoding('utf8');
  client.on('error', common.mustNotCall());
  client.on('end', common.mustCall(() => {
    assert.strictEqual(
      response,
      'HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n'
    );
    server.close();
  }));
  client.write(msg);
  client.resume();
}));
  client.on('end', common.mustCall(() => {
    assert.strictEqual(
      response,
      'HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n'
    );
    server.close();
  }));
  client.write(msg);
  client.resume();
}));