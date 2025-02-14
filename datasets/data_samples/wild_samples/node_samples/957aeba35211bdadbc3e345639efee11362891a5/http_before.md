server.on('clientError', (err, socket) => {
  socket.end('HTTP/1.1 400 Bad Request\r\n\r\n');
});
server.listen(8000);
```

When the `'clientError'` event occurs, there is no `request` or `response`
object, so any HTTP response sent, including response headers and payload,
*must* be written directly to the `socket` object. Care must be taken to
ensure the response is a properly formatted HTTP response message.

`err` is an instance of `Error` with two extra columns:

+ `bytesParsed`: the bytes count of request packet that Node.js may have parsed
  correctly;
+ `rawPacket`: the raw packet of current request.

### Event: 'close'
<!-- YAML
added: v0.1.4
-->

Emitted when the server closes.

### Event: 'connect'
<!-- YAML
added: v0.7.0
-->

* `request` {http.IncomingMessage} Arguments for the HTTP request, as it is in
  the [`'request'`][] event
* `socket` {net.Socket} Network socket between the server and client
* `head` {Buffer} The first packet of the tunneling stream (may be empty)

Emitted each time a client requests an HTTP `CONNECT` method. If this event is
not listened for, then clients requesting a `CONNECT` method will have their
connections closed.

After this event is emitted, the request's socket will not have a `'data'`
event listener, meaning it will need to be bound in order to handle data
sent to the server on that socket.

### Event: 'connection'
<!-- YAML
added: v0.1.0
-->

* `socket` {net.Socket}

This event is emitted when a new TCP stream is established. `socket` is
typically an object of type [`net.Socket`][]. Usually users will not want to
access this event. In particular, the socket will not emit `'readable'` events
because of how the protocol parser attaches to the socket. The `socket` can
also be accessed at `request.connection`.

This event can also be explicitly emitted by users to inject connections
into the HTTP server. In that case, any [`Duplex`][] stream can be passed.

### Event: 'request'
<!-- YAML
added: v0.1.0
-->

* `request` {http.IncomingMessage}
* `response` {http.ServerResponse}

Emitted each time there is a request. Note that there may be multiple requests
per connection (in the case of HTTP Keep-Alive connections).

### Event: 'upgrade'
<!-- YAML
added: v0.1.94
changes:
  - version: v10.0.0
    pr-url: v10.0.0
    description: Not listening to this event no longer causes the socket
                 to be destroyed if a client sends an Upgrade header.
-->

* `request` {http.IncomingMessage} Arguments for the HTTP request, as it is in
  the [`'request'`][] event
* `socket` {net.Socket} Network socket between the server and client
* `head` {Buffer} The first packet of the upgraded stream (may be empty)

Emitted each time a client requests an HTTP upgrade. Listening to this event
is optional and clients cannot insist on a protocol change.

After this event is emitted, the request's socket will not have a `'data'`
event listener, meaning it will need to be bound in order to handle data
sent to the server on that socket.

### server.close([callback])
<!-- YAML
added: v0.1.90
-->

* `callback` {Function}

Stops the server from accepting new connections. See [`net.Server.close()`][].

### server.listen()

Starts the HTTP server listening for connections.
This method is identical to [`server.listen()`][] from [`net.Server`][].

### server.listening
<!-- YAML
added: v5.7.0
-->

* {boolean} Indicates whether or not the server is listening for connections.

### server.maxHeadersCount
<!-- YAML
added: v0.7.0
-->

* {number} **Default:** `2000`

Limits maximum incoming headers count. If set to 0, no limit will be applied.

### server.headersTimeout
<!-- YAML
added: REPLACEME
-->

* {number} **Default:** `40000`

Limit the amount of time the parser will wait to receive the complete HTTP
headers.

In case of inactivity, the rules defined in [server.timeout][] apply. However,
that inactivity based timeout would still allow the connection to be kept open
if the headers are being sent very slowly (by default, up to a byte per 2
minutes). In order to prevent this, whenever header data arrives an additional
check is made that more than `server.headersTimeout` milliseconds has not
passed since the connection was established. If the check fails, a `'timeout'`
event is emitted on the server object, and (by default) the socket is destroyed.
See [server.timeout][] for more information on how timeout behaviour can be
customised.

### server.setTimeout([msecs][, callback])
<!-- YAML
added: v0.9.12
-->

* `msecs` {number} **Default:** `120000` (2 minutes)
* `callback` {Function}
* Returns: {http.Server}

Sets the timeout value for sockets, and emits a `'timeout'` event on
the Server object, passing the socket as an argument, if a timeout
occurs.

If there is a `'timeout'` event listener on the Server object, then it
will be called with the timed-out socket as an argument.

By default, the Server's timeout value is 2 minutes, and sockets are
destroyed automatically if they time out. However, if a callback is assigned
to the Server's `'timeout'` event, timeouts must be handled explicitly.

### server.timeout
<!-- YAML
added: v0.9.12
-->

* {number} Timeout in milliseconds. **Default:** `120000` (2 minutes).

The number of milliseconds of inactivity before a socket is presumed
to have timed out.

A value of `0` will disable the timeout behavior on incoming connections.

The socket timeout logic is set up on connection, so changing this
value only affects new connections to the server, not any existing connections.

### server.keepAliveTimeout
<!-- YAML
added: v8.0.0
-->

* {number} Timeout in milliseconds. **Default:** `5000` (5 seconds).

The number of milliseconds of inactivity a server needs to wait for additional
incoming data, after it has finished writing the last response, before a socket
will be destroyed. If the server receives new data before the keep-alive
timeout has fired, it will reset the regular inactivity timeout, i.e.,
[`server.timeout`][].

A value of `0` will disable the keep-alive timeout behavior on incoming
connections.
A value of `0` makes the http server behave similarly to Node.js versions prior
to 8.0.0, which did not have a keep-alive timeout.

The socket timeout logic is set up on connection, so changing this value only
affects new connections to the server, not any existing connections.

## Class: http.ServerResponse
<!-- YAML
added: v0.1.17
-->

This object is created internally by an HTTP server — not by the user. It is
passed as the second parameter to the [`'request'`][] event.

The response inherits from [Stream][], and additionally implements the
following:

### Event: 'close'
<!-- YAML
added: v0.6.7
-->

Indicates that the underlying connection was terminated.

### Event: 'finish'
<!-- YAML
added: v0.3.6
-->

Emitted when the response has been sent. More specifically, this event is
emitted when the last segment of the response headers and body have been
handed off to the operating system for transmission over the network. It
does not imply that the client has received anything yet.

### response.addTrailers(headers)
<!-- YAML
added: v0.3.0
-->

* `headers` {Object}

This method adds HTTP trailing headers (a header but at the end of the
message) to the response.

Trailers will **only** be emitted if chunked encoding is used for the
response; if it is not (e.g. if the request was HTTP/1.0), they will
be silently discarded.

Note that HTTP requires the `Trailer` header to be sent in order to
emit trailers, with a list of the header fields in its value. E.g.,

```js
response.writeHead(200, { 'Content-Type': 'text/plain',
                          'Trailer': 'Content-MD5' });
response.write(fileData);
response.addTrailers({ 'Content-MD5': '7895bf4b8828b55ceaf47747b4bca667' });
response.end();
```

Attempting to set a header field name or value that contains invalid characters
will result in a [`TypeError`][] being thrown.

### response.connection
<!-- YAML
added: v0.3.0
-->

* {net.Socket}

See [`response.socket`][].

### response.end([data][, encoding][, callback])
<!-- YAML
added: v0.1.90
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18780
    description: This method now returns a reference to `ServerResponse`.
-->

* `data` {string|Buffer}
* `encoding` {string}
* `callback` {Function}
* Returns: {this}

This method signals to the server that all of the response headers and body
have been sent; that server should consider this message complete.
The method, `response.end()`, MUST be called on each response.

If `data` is specified, it is equivalent to calling
[`response.write(data, encoding)`][] followed by `response.end(callback)`.

If `callback` is specified, it will be called when the response stream
is finished.

### response.finished
<!-- YAML
added: v0.0.2
-->

* {boolean}

Boolean value that indicates whether the response has completed. Starts
as `false`. After [`response.end()`][] executes, the value will be `true`.

### response.getHeader(name)
<!-- YAML
added: v0.4.0
-->

* `name` {string}
* Returns: {any}

Reads out a header that's already been queued but not sent to the client.
Note that the name is case insensitive. The type of the return value depends
on the arguments provided to [`response.setHeader()`][].

```js
response.setHeader('Content-Type', 'text/html');
response.setHeader('Content-Length', Buffer.byteLength(body));
response.setHeader('Set-Cookie', ['type=ninja', 'language=javascript']);
const contentType = response.getHeader('content-type');
// contentType is 'text/html'
const contentLength = response.getHeader('Content-Length');
// contentLength is of type number
const setCookie = response.getHeader('set-cookie');
// setCookie is of type string[]
```

### response.getHeaderNames()
<!-- YAML
added: v7.7.0
-->

* Returns: {string[]}

Returns an array containing the unique names of the current outgoing headers.
All header names are lowercase.

```js
response.setHeader('Foo', 'bar');
response.setHeader('Set-Cookie', ['foo=bar', 'bar=baz']);

const headerNames = response.getHeaderNames();
// headerNames === ['foo', 'set-cookie']
```

### response.getHeaders()
<!-- YAML
added: v7.7.0
-->

* Returns: {Object}

Returns a shallow copy of the current outgoing headers. Since a shallow copy
is used, array values may be mutated without additional calls to various
header-related http module methods. The keys of the returned object are the
header names and the values are the respective header values. All header names
are lowercase.

The object returned by the `response.getHeaders()` method _does not_
prototypically inherit from the JavaScript `Object`. This means that typical
`Object` methods such as `obj.toString()`, `obj.hasOwnProperty()`, and others
are not defined and *will not work*.

```js
response.setHeader('Foo', 'bar');
response.setHeader('Set-Cookie', ['foo=bar', 'bar=baz']);

const headers = response.getHeaders();
// headers === { foo: 'bar', 'set-cookie': ['foo=bar', 'bar=baz'] }
```

### response.hasHeader(name)
<!-- YAML
added: v7.7.0
-->

* `name` {string}
* Returns: {boolean}

Returns `true` if the header identified by `name` is currently set in the
outgoing headers. Note that the header name matching is case-insensitive.

```js
const hasContentType = response.hasHeader('content-type');
```

### response.headersSent
<!-- YAML
added: v0.9.3
-->

* {boolean}

Boolean (read-only). True if headers were sent, false otherwise.

### response.removeHeader(name)
<!-- YAML
added: v0.4.0
-->

* `name` {string}

Removes a header that's queued for implicit sending.

```js
response.removeHeader('Content-Encoding');
```

### response.sendDate
<!-- YAML
added: v0.7.5
-->

* {boolean}

When true, the Date header will be automatically generated and sent in
the response if it is not already present in the headers. Defaults to true.

This should only be disabled for testing; HTTP requires the Date header
in responses.

### response.setHeader(name, value)
<!-- YAML
added: v0.4.0
-->

* `name` {string}
* `value` {any}

Sets a single header value for implicit headers. If this header already exists
in the to-be-sent headers, its value will be replaced. Use an array of strings
here to send multiple headers with the same name. Non-string values will be
stored without modification. Therefore, [`response.getHeader()`][] may return
non-string values. However, the non-string values will be converted to strings
for network transmission.

```js
response.setHeader('Content-Type', 'text/html');
```

or

```js
response.setHeader('Set-Cookie', ['type=ninja', 'language=javascript']);
```

Attempting to set a header field name or value that contains invalid characters
will result in a [`TypeError`][] being thrown.

When headers have been set with [`response.setHeader()`][], they will be merged
with any headers passed to [`response.writeHead()`][], with the headers passed
to [`response.writeHead()`][] given precedence.

```js
// returns content-type = text/plain
const server = http.createServer((req, res) => {
  res.setHeader('Content-Type', 'text/html');
  res.setHeader('X-Foo', 'bar');
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('ok');
});
```

If [`response.writeHead()`][] method is called and this method has not been
called, it will directly write the supplied header values onto the network
channel without caching internally, and the [`response.getHeader()`][] on the
header will not yield the expected result. If progressive population of headers
is desired with potential future retrieval and modification, use
[`response.setHeader()`][] instead of [`response.writeHead()`][].

### response.setTimeout(msecs[, callback])
<!-- YAML
added: v0.9.12
-->

* `msecs` {number}
* `callback` {Function}
* Returns: {http.ServerResponse}

Sets the Socket's timeout value to `msecs`. If a callback is
provided, then it is added as a listener on the `'timeout'` event on
the response object.

If no `'timeout'` listener is added to the request, the response, or
the server, then sockets are destroyed when they time out. If a handler is
assigned to the request, the response, or the server's `'timeout'` events,
timed out sockets must be handled explicitly.

### response.socket
<!-- YAML
added: v0.3.0
-->

* {net.Socket}

Reference to the underlying socket. Usually users will not want to access
this property. In particular, the socket will not emit `'readable'` events
because of how the protocol parser attaches to the socket. After
`response.end()`, the property is nulled. The `socket` may also be accessed
via `response.connection`.

```js
const http = require('http');
const server = http.createServer((req, res) => {
  const ip = res.socket.remoteAddress;
  const port = res.socket.remotePort;
  res.end(`Your IP address is ${ip} and your source port is ${port}.`);
}).listen(3000);
```

### response.statusCode
<!-- YAML
added: v0.4.0
-->

* {number}

When using implicit headers (not calling [`response.writeHead()`][] explicitly),
this property controls the status code that will be sent to the client when
the headers get flushed.

```js
response.statusCode = 404;
```

After response header was sent to the client, this property indicates the
status code which was sent out.

### response.statusMessage
<!-- YAML
added: v0.11.8
-->

* {string}

When using implicit headers (not calling [`response.writeHead()`][] explicitly),
this property controls the status message that will be sent to the client when
the headers get flushed. If this is left as `undefined` then the standard
message for the status code will be used.

```js
response.statusMessage = 'Not found';
```

After response header was sent to the client, this property indicates the
status message which was sent out.

### response.write(chunk[, encoding][, callback])
<!-- YAML
added: v0.1.29
-->

* `chunk` {string|Buffer}
* `encoding` {string} **Default:** `'utf8'`
* `callback` {Function}
* Returns: {boolean}

If this method is called and [`response.writeHead()`][] has not been called,
it will switch to implicit header mode and flush the implicit headers.

This sends a chunk of the response body. This method may
be called multiple times to provide successive parts of the body.

Note that in the `http` module, the response body is omitted when the
request is a HEAD request. Similarly, the `204` and `304` responses
_must not_ include a message body.

`chunk` can be a string or a buffer. If `chunk` is a string,
the second parameter specifies how to encode it into a byte stream.
`callback` will be called when this chunk of data is flushed.

This is the raw HTTP body and has nothing to do with higher-level multi-part
body encodings that may be used.

The first time [`response.write()`][] is called, it will send the buffered
header information and the first chunk of the body to the client. The second
time [`response.write()`][] is called, Node.js assumes data will be streamed,
and sends the new data separately. That is, the response is buffered up to the
first chunk of the body.

Returns `true` if the entire data was flushed successfully to the kernel
buffer. Returns `false` if all or part of the data was queued in user memory.
`'drain'` will be emitted when the buffer is free again.

### response.writeContinue()
<!-- YAML
added: v0.3.0
-->

Sends a HTTP/1.1 100 Continue message to the client, indicating that
the request body should be sent. See the [`'checkContinue'`][] event on
`Server`.

### response.writeHead(statusCode[, statusMessage][, headers])
<!-- YAML
added: v0.1.30
changes:
  - version: v5.11.0, v4.4.5
    pr-url: https://github.com/nodejs/node/pull/6291
    description: A `RangeError` is thrown if `statusCode` is not a number in
                 the range `[100, 999]`.
-->

* `statusCode` {number}
* `statusMessage` {string}
* `headers` {Object}

Sends a response header to the request. The status code is a 3-digit HTTP
status code, like `404`. The last argument, `headers`, are the response headers.
Optionally one can give a human-readable `statusMessage` as the second
argument.

```js
const body = 'hello world';
response.writeHead(200, {
  'Content-Length': Buffer.byteLength(body),
  'Content-Type': 'text/plain' });
```

This method must only be called once on a message and it must
be called before [`response.end()`][] is called.

If [`response.write()`][] or [`response.end()`][] are called before calling
this, the implicit/mutable headers will be calculated and call this function.

When headers have been set with [`response.setHeader()`][], they will be merged
with any headers passed to [`response.writeHead()`][], with the headers passed
to [`response.writeHead()`][] given precedence.

If this method is called and [`response.setHeader()`][] has not been called,
it will directly write the supplied header values onto the network channel
without caching internally, and the [`response.getHeader()`][] on the header
will not yield the expected result. If progressive population of headers is
desired with potential future retrieval and modification, use
[`response.setHeader()`][] instead.

```js
// returns content-type = text/plain
const server = http.createServer((req, res) => {
  res.setHeader('Content-Type', 'text/html');
  res.setHeader('X-Foo', 'bar');
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('ok');
});
```

Note that Content-Length is given in bytes not characters. The above example
works because the string `'hello world'` contains only single byte characters.
If the body contains higher coded characters then `Buffer.byteLength()`
should be used to determine the number of bytes in a given encoding.
And Node.js does not check whether Content-Length and the length of the body
which has been transmitted are equal or not.

Attempting to set a header field name or value that contains invalid characters
will result in a [`TypeError`][] being thrown.

### response.writeProcessing()
<!-- YAML
added: v10.0.0
-->

Sends a HTTP/1.1 102 Processing message to the client, indicating that
the request body should be sent.

## Class: http.IncomingMessage
<!-- YAML
added: v0.1.17
-->

An `IncomingMessage` object is created by [`http.Server`][] or
[`http.ClientRequest`][] and passed as the first argument to the [`'request'`][]
and [`'response'`][] event respectively. It may be used to access response
status, headers and data.

It implements the [Readable Stream][] interface, as well as the
following additional events, methods, and properties.

### Event: 'aborted'
<!-- YAML
added: v0.3.8
-->

Emitted when the request has been aborted.

### Event: 'close'
<!-- YAML
added: v0.4.2
-->

Indicates that the underlying connection was closed.

### message.aborted
<!-- YAML
added: v10.1.0
-->

* {boolean}

The `message.aborted` property will be `true` if the request has
been aborted.

### message.complete
<!-- YAML
added: v0.3.0
-->

* {boolean}

The `message.complete` property will be `true` if a complete HTTP message has
been received and successfully parsed.

This property is particularly useful as a means of determining if a client or
server fully transmitted a message before a connection was terminated:

```js
const req = http.request({
  host: '127.0.0.1',
  port: 8080,
  method: 'POST'
}, (res) => {
  res.resume();
  res.on('end', () => {
    if (!res.complete)
      console.error(
        'The connection was terminated while the message was still being sent');
  });
});
```

### message.destroy([error])
<!-- YAML
added: v0.3.0
-->

* `error` {Error}

Calls `destroy()` on the socket that received the `IncomingMessage`. If `error`
is provided, an `'error'` event is emitted and `error` is passed as an argument
to any listeners on the event.

### message.headers
<!-- YAML
added: v0.1.5
-->

* {Object}

The request/response headers object.

Key-value pairs of header names and values. Header names are lower-cased.

```js
// Prints something like:
//
// { 'user-agent': 'curl/7.22.0',
//   host: '127.0.0.1:8000',
//   accept: '*/*' }
console.log(request.headers);
```

Duplicates in raw headers are handled in the following ways, depending on the
header name:

* Duplicates of `age`, `authorization`, `content-length`, `content-type`,
`etag`, `expires`, `from`, `host`, `if-modified-since`, `if-unmodified-since`,
`last-modified`, `location`, `max-forwards`, `proxy-authorization`, `referer`,
`retry-after`, or `user-agent` are discarded.
* `set-cookie` is always an array. Duplicates are added to the array.
* For all other headers, the values are joined together with ', '.

### message.httpVersion
<!-- YAML
added: v0.1.1
-->

* {string}

In case of server request, the HTTP version sent by the client. In the case of
client response, the HTTP version of the connected-to server.
Probably either `'1.1'` or `'1.0'`.

Also `message.httpVersionMajor` is the first integer and
`message.httpVersionMinor` is the second.

### message.method
<!-- YAML
added: v0.1.1
-->

* {string}

**Only valid for request obtained from [`http.Server`][].**

The request method as a string. Read only. Examples: `'GET'`, `'DELETE'`.

### message.rawHeaders
<!-- YAML
added: v0.11.6
-->

* {string[]}

The raw request/response headers list exactly as they were received.

Note that the keys and values are in the same list. It is *not* a
list of tuples. So, the even-numbered offsets are key values, and the
odd-numbered offsets are the associated values.

Header names are not lowercased, and duplicates are not merged.

```js
// Prints something like:
//
// [ 'user-agent',
//   'this is invalid because there can be only one',
//   'User-Agent',
//   'curl/7.22.0',
//   'Host',
//   '127.0.0.1:8000',
//   'ACCEPT',
//   '*/*' ]
console.log(request.rawHeaders);
```

### message.rawTrailers
<!-- YAML
added: v0.11.6
-->

* {string[]}

The raw request/response trailer keys and values exactly as they were
received. Only populated at the `'end'` event.

### message.setTimeout(msecs, callback)
<!-- YAML
added: v0.5.9
-->

* `msecs` {number}
* `callback` {Function}
* Returns: {http.IncomingMessage}

Calls `message.connection.setTimeout(msecs, callback)`.

### message.socket
<!-- YAML
added: v0.3.0
-->

* {net.Socket}

The [`net.Socket`][] object associated with the connection.

With HTTPS support, use [`request.socket.getPeerCertificate()`][] to obtain the
client's authentication details.

### message.statusCode
<!-- YAML
added: v0.1.1
-->

* {number}

**Only valid for response obtained from [`http.ClientRequest`][].**

The 3-digit HTTP response status code. E.G. `404`.

### message.statusMessage
<!-- YAML
added: v0.11.10
-->

* {string}

**Only valid for response obtained from [`http.ClientRequest`][].**

The HTTP response status message (reason phrase). E.G. `OK` or `Internal Server
Error`.

### message.trailers
<!-- YAML
added: v0.3.0
-->

* {Object}

The request/response trailers object. Only populated at the `'end'` event.

### message.url
<!-- YAML
added: v0.1.90
-->

* {string}

**Only valid for request obtained from [`http.Server`][].**

Request URL string. This contains only the URL that is
present in the actual HTTP request. If the request is:

```txt
GET /status?name=ryan HTTP/1.1\r\n
Accept: text/plain\r\n
\r\n
```

Then `request.url` will be:

<!-- eslint-disable semi -->
```js
'/status?name=ryan'
```

To parse the url into its parts `require('url').parse(request.url)`
can be used:

```txt
$ node
> require('url').parse('/status?name=ryan')
Url {
  protocol: null,
  slashes: null,
  auth: null,
  host: null,
  port: null,
  hostname: null,
  hash: null,
  search: '?name=ryan',
  query: 'name=ryan',
  pathname: '/status',
  path: '/status?name=ryan',
  href: '/status?name=ryan' }
```

To extract the parameters from the query string, the
`require('querystring').parse` function can be used, or
`true` can be passed as the second argument to `require('url').parse`:

```txt
$ node
> require('url').parse('/status?name=ryan', true)
Url {
  protocol: null,
  slashes: null,
  auth: null,
  host: null,
  port: null,
  hostname: null,
  hash: null,
  search: '?name=ryan',
  query: { name: 'ryan' },
  pathname: '/status',
  path: '/status?name=ryan',
  href: '/status?name=ryan' }
```

## http.METHODS
<!-- YAML
added: v0.11.8
-->

* {string[]}

A list of the HTTP methods that are supported by the parser.

## http.STATUS_CODES
<!-- YAML
added: v0.1.22
-->

* {Object}

A collection of all the standard HTTP response status codes, and the
short description of each. For example, `http.STATUS_CODES[404] === 'Not
Found'`.

## http.createServer([options][, requestListener])
<!-- YAML
added: v0.1.13
changes:
  - version: v9.6.0
    pr-url: https://github.com/nodejs/node/pull/15752
    description: The `options` argument is supported now.
-->
* `options` {Object}
  * `IncomingMessage` {http.IncomingMessage} Specifies the `IncomingMessage`
    class to be used. Useful for extending the original `IncomingMessage`.
    **Default:** `IncomingMessage`.
  * `ServerResponse` {http.ServerResponse} Specifies the `ServerResponse` class
    to be used. Useful for extending the original `ServerResponse`. **Default:**
    `ServerResponse`.
* `requestListener` {Function}

* Returns: {http.Server}

Returns a new instance of [`http.Server`][].

The `requestListener` is a function which is automatically
added to the [`'request'`][] event.

## http.get(options[, callback])
## http.get(url[, options][, callback])
<!-- YAML
added: v0.3.6
changes:
  - version: v10.9.0
    pr-url: https://github.com/nodejs/node/pull/21616
    description: The `url` parameter can now be passed along with a separate
                 `options` object.
  - version: v7.5.0
    pr-url: https://github.com/nodejs/node/pull/10638
    description: The `options` parameter can be a WHATWG `URL` object.
-->

* `url` {string | URL}
* `options` {Object} Accepts the same `options` as
  [`http.request()`][], with the `method` always set to `GET`.
  Properties that are inherited from the prototype are ignored.
* `callback` {Function}
* Returns: {http.ClientRequest}

Since most requests are GET requests without bodies, Node.js provides this
convenience method. The only difference between this method and
[`http.request()`][] is that it sets the method to GET and calls `req.end()`
automatically. Note that the callback must take care to consume the response
data for reasons stated in [`http.ClientRequest`][] section.

The `callback` is invoked with a single argument that is an instance of
[`http.IncomingMessage`][].

JSON fetching example:

```js
http.get('http://nodejs.org/dist/index.json', (res) => {
  const { statusCode } = res;
  const contentType = res.headers['content-type'];

  let error;
  if (statusCode !== 200) {
    error = new Error('Request Failed.\n' +
                      `Status Code: ${statusCode}`);
  } else if (!/^application\/json/.test(contentType)) {
    error = new Error('Invalid content-type.\n' +
                      `Expected application/json but received ${contentType}`);
  }
  if (error) {
    console.error(error.message);
    // consume response data to free up memory
    res.resume();
    return;
  }

  res.setEncoding('utf8');
  let rawData = '';
  res.on('data', (chunk) => { rawData += chunk; });
  res.on('end', () => {
    try {
      const parsedData = JSON.parse(rawData);
      console.log(parsedData);
    } catch (e) {
      console.error(e.message);
    }
  });
}).on('error', (e) => {
  console.error(`Got error: ${e.message}`);
});
```

## http.globalAgent
<!-- YAML
added: v0.5.9
-->

* {http.Agent}

Global instance of `Agent` which is used as the default for all HTTP client
requests.

## http.request(options[, callback])
## http.request(url[, options][, callback])
<!-- YAML
added: v0.3.6
changes:
  - version: v10.9.0
    pr-url: https://github.com/nodejs/node/pull/21616
    description: The `url` parameter can now be passed along with a separate
                 `options` object.
  - version: v7.5.0
    pr-url: https://github.com/nodejs/node/pull/10638
    description: The `options` parameter can be a WHATWG `URL` object.
-->

* `url` {string | URL}
* `options` {Object}
  * `protocol` {string} Protocol to use. **Default:** `'http:'`.
  * `host` {string} A domain name or IP address of the server to issue the
    request to. **Default:** `'localhost'`.
  * `hostname` {string} Alias for `host`. To support [`url.parse()`][],
    `hostname` will be used if both `host` and `hostname` are specified.
  * `family` {number} IP address family to use when resolving `host` or
    `hostname`. Valid values are `4` or `6`. When unspecified, both IP v4 and
    v6 will be used.
  * `port` {number} Port of remote server. **Default:** `80`.
  * `localAddress` {string} Local interface to bind for network connections.
  * `socketPath` {string} Unix Domain Socket (cannot be used if one of `host`
     or `port` is specified, those specify a TCP Socket).
  * `method` {string} A string specifying the HTTP request method. **Default:**
    `'GET'`.
  * `path` {string} Request path. Should include query string if any.
    E.G. `'/index.html?page=12'`. An exception is thrown when the request path
    contains illegal characters. Currently, only spaces are rejected but that
    may change in the future. **Default:** `'/'`.
  * `headers` {Object} An object containing request headers.
  * `auth` {string} Basic authentication i.e. `'user:password'` to compute an
    Authorization header.
  * `agent` {http.Agent | boolean} Controls [`Agent`][] behavior. Possible
    values:
    * `undefined` (default): use [`http.globalAgent`][] for this host and port.
    * `Agent` object: explicitly use the passed in `Agent`.
    * `false`: causes a new `Agent` with default values to be used.
  * `createConnection` {Function} A function that produces a socket/stream to
    use for the request when the `agent` option is not used. This can be used to
    avoid creating a custom `Agent` class just to override the default
    `createConnection` function. See [`agent.createConnection()`][] for more
    details. Any [`Duplex`][] stream is a valid return value.
  * `timeout` {number}: A number specifying the socket timeout in milliseconds.
    This will set the timeout before the socket is connected.
  * `setHost` {boolean}: Specifies whether or not to automatically add the
    `Host` header. Defaults to `true`.
* `callback` {Function}
* Returns: {http.ClientRequest}

Node.js maintains several connections per server to make HTTP requests.
This function allows one to transparently issue requests.

`url` can be a string or a [`URL`][] object. If `url` is a
string, it is automatically parsed with [`new URL()`][]. If it is a [`URL`][]
object, it will be automatically converted to an ordinary `options` object.

If both `url` and `options` are specified, the objects are merged, with the
`options` properties taking precedence.

The optional `callback` parameter will be added as a one-time listener for
the [`'response'`][] event.

`http.request()` returns an instance of the [`http.ClientRequest`][]
class. The `ClientRequest` instance is a writable stream. If one needs to
upload a file with a POST request, then write to the `ClientRequest` object.

```js
const postData = querystring.stringify({
  'msg': 'Hello World!'
});

const options = {
  hostname: 'www.google.com',
  port: 80,
  path: '/upload',
  method: 'POST',
  headers: {
    'Content-Type': 'application/x-www-form-urlencoded',
    'Content-Length': Buffer.byteLength(postData)
  }
};

const req = http.request(options, (res) => {
  console.log(`STATUS: ${res.statusCode}`);
  console.log(`HEADERS: ${JSON.stringify(res.headers)}`);
  res.setEncoding('utf8');
  res.on('data', (chunk) => {
    console.log(`BODY: ${chunk}`);
  });
  res.on('end', () => {
    console.log('No more data in response.');
  });
});

req.on('error', (e) => {
  console.error(`problem with request: ${e.message}`);
});

// write data to request body
req.write(postData);
req.end();
```

Note that in the example `req.end()` was called. With `http.request()` one
must always call `req.end()` to signify the end of the request -
even if there is no data being written to the request body.

If any error is encountered during the request (be that with DNS resolution,
TCP level errors, or actual HTTP parse errors) an `'error'` event is emitted
on the returned request object. As with all `'error'` events, if no listeners
are registered the error will be thrown.

There are a few special headers that should be noted.

* Sending a 'Connection: keep-alive' will notify Node.js that the connection to
  the server should be persisted until the next request.

* Sending a 'Content-Length' header will disable the default chunked encoding.

* Sending an 'Expect' header will immediately send the request headers.
  Usually, when sending 'Expect: 100-continue', both a timeout and a listener
  for the `'continue'` event should be set. See RFC2616 Section 8.2.3 for more
  information.

* Sending an Authorization header will override using the `auth` option
  to compute basic authentication.

Example using a [`URL`][] as `options`:

```js
const options = new URL('http://abc:xyz@example.com');

const req = http.request(options, (res) => {
  // ...
});
```

In a successful request, the following events will be emitted in the following
order:

* `'socket'`
* `'response'`
  * `'data'` any number of times, on the `res` object
    (`'data'` will not be emitted at all if the response body is empty, for
    instance, in most redirects)
  * `'end'` on the `res` object
* `'close'`

In the case of a connection error, the following events will be emitted:

* `'socket'`
* `'error'`
* `'close'`

If `req.abort()` is called before the connection succeeds, the following events
will be emitted in the following order:

* `'socket'`
* (`req.abort()` called here)
* `'abort'`
* `'close'`
* `'error'` with an error with message `'Error: socket hang up'` and code
  `'ECONNRESET'`

If `req.abort()` is called after the response is received, the following events
will be emitted in the following order:

* `'socket'`
* `'response'`
  * `'data'` any number of times, on the `res` object
* (`req.abort()` called here)
* `'abort'`
* `'close'`
  * `'aborted'` on the `res` object
  * `'end'` on the `res` object
  * `'close'` on the `res` object

Note that setting the `timeout` option or using the `setTimeout()` function will
not abort the request or do anything besides add a `'timeout'` event.

[`'checkContinue'`]: #http_event_checkcontinue
[`'request'`]: #http_event_request
[`'response'`]: #http_event_response
[`'upgrade'`]: #http_event_upgrade
[`Agent`]: #http_class_http_agent
[`Duplex`]: stream.html#stream_class_stream_duplex
[`TypeError`]: errors.html#errors_class_typeerror
[`URL`]: url.html#url_the_whatwg_url_api
[`agent.createConnection()`]: #http_agent_createconnection_options_callback
[`agent.getName()`]: #http_agent_getname_options
[`destroy()`]: #http_agent_destroy
[`getHeader(name)`]: #http_request_getheader_name
[`http.Agent`]: #http_class_http_agent
[`http.ClientRequest`]: #http_class_http_clientrequest
[`http.IncomingMessage`]: #http_class_http_incomingmessage
[`http.Server`]: #http_class_http_server
[`http.get()`]: #http_http_get_options_callback
[`http.globalAgent`]: #http_http_globalagent
[`http.request()`]: #http_http_request_options_callback
[`message.headers`]: #http_message_headers
[`net.Server.close()`]: net.html#net_server_close_callback
[`net.Server`]: net.html#net_class_net_server
[`net.Socket`]: net.html#net_class_net_socket
[`net.createConnection()`]: net.html#net_net_createconnection_options_connectlistener
[`new URL()`]: url.html#url_constructor_new_url_input_base
[`removeHeader(name)`]: #http_request_removeheader_name
[`request.end()`]: #http_request_end_data_encoding_callback
[`request.getHeader()`]: #http_request_getheader_name
[`request.setHeader()`]: #http_request_setheader_name_value
[`request.setTimeout()`]: #http_request_settimeout_timeout_callback
[`request.socket`]: #http_request_socket
[`request.socket.getPeerCertificate()`]: tls.html#tls_tlssocket_getpeercertificate_detailed
[`request.write(data, encoding)`]: #http_request_write_chunk_encoding_callback
[`response.end()`]: #http_response_end_data_encoding_callback
[`response.getHeader()`]: #http_response_getheader_name
[`response.setHeader()`]: #http_response_setheader_name_value
[`response.socket`]: #http_response_socket
[`response.write()`]: #http_response_write_chunk_encoding_callback
[`response.write(data, encoding)`]: #http_response_write_chunk_encoding_callback
[`response.writeContinue()`]: #http_response_writecontinue
[`response.writeHead()`]: #http_response_writehead_statuscode_statusmessage_headers
[`server.listen()`]: net.html#net_server_listen
[`server.timeout`]: #http_server_timeout
[`setHeader(name, value)`]: #http_request_setheader_name_value
[`socket.setKeepAlive()`]: net.html#net_socket_setkeepalive_enable_initialdelay
[`socket.setNoDelay()`]: net.html#net_socket_setnodelay_nodelay
[`socket.setTimeout()`]: net.html#net_socket_settimeout_timeout_callback
[`socket.unref()`]: net.html#net_socket_unref
[`url.parse()`]: url.html#url_url_parse_urlstring_parsequerystring_slashesdenotehost
[Readable Stream]: stream.html#stream_class_stream_readable
[Stream]: stream.html#stream_stream