node-vorbis
========
### NodeJS native binding to libvorbis

This module provides Vorbis Encoder and Decoder classes compatible with `node-ogg`
streams.

Installation
------------

`node-vorbis` comes bundled with its own copy of `libvorbis`, so
there's no need to have the library pre-installed on your system.

Simply compile and install `node-vorbis` using `npm`:

``` bash
$ npm install vorbis
```


Example
-------

``` javascript
var fs = require('fs');
var ogg = require('ogg');
var vorbis = require('vorbis');
var file = __dirname + '/Hydrate-Kenny_Beltrey.ogg';

var od = new ogg.Decoder();
od.on('stream', function (stream) {
  var vd = new vorbis.Decoder();
  stream.use(vd);

  // the "format" event contains the raw PCM format
  vd.on('format', function (format) {
    // send the raw PCM data to stdout
    vd.pipe(process.stdout);
  });

  // an "error" event will get emitted if the stream is not a Vorbis stream
  // (i.e. it could be a Theora video stream instead)
  vd.on('error', function (err) {
    // maybe try another decoder...
  });
});

fs.createReadStream(file).pipe(od);
```

See the `examples` directory for some more example code.

API
---

### Decoder class


### Encoder class
