//
// Echo server with Keep Alive support
//
const http = require("http");

const PORT = 8080;

const server = http.createServer((req, res) => {
  let userAgent = req.headers['user-agent'];
  if (req.method == 'GET') {
    res.end(`hello ${userAgent}\n`);
  } else {
    req.on('data', function (chunk) {
      console.log(chunk.length);
      res.end(chunk);
    });
  }
});

server.keepAliveTimeout = 10;
server.headersTimeout = 10 * 1000;

server.listen(PORT, () => {
  const { address, port } = server.address();
  console.log("Listening on %s:%d", address, port);
});
