const { createServer, maxHeaderSize } = require('http');
const { createConnection } = require('net');

const { getOptionValue } = require('internal/options');

const CRLF = '\r\n';
const DUMMY_HEADER_NAME = 'Cookie: ';
const DUMMY_HEADER_VALUE = 'a'.repeat(
  // Plus one is to make it 1 byte too big
  maxHeaderSize - DUMMY_HEADER_NAME.length - (2 * CRLF.length) + 1
);
const PAYLOAD_GET = 'GET /blah HTTP/1.1';
const PAYLOAD = PAYLOAD_GET + CRLF +
  DUMMY_HEADER_NAME + DUMMY_HEADER_VALUE + CRLF.repeat(2);
const server = createServer();

server.on('connection', mustCall((socket) => {
  // Legacy parser gives sligthly different response.
  // This discripancy is not fixed on purpose.
  const legacy = getOptionValue('--http-parser') === 'legacy';
  socket.on('error', expectsError({
    name: 'Error',
    message: 'Parse Error: Header overflow',
    code: 'HPE_HEADER_OVERFLOW',
    bytesParsed: maxHeaderSize + PAYLOAD_GET.length - (legacy ? -1 : 0),
    rawPacket: Buffer.from(PAYLOAD)
  }));
}));
