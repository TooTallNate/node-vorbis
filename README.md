node-vorbis
========
### NodeJS native binding to libvorbis
[![Build Status](https://travis-ci.org/TooTallNate/node-vorbis.svg?branch=master)](https://travis-ci.org/TooTallNate/node-vorbis)

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

Decoder example:

``` javascript
var fs = require('fs');
var ogg = require('ogg');
var vorbis = require('vorbis');
var file = __dirname + '/Hydrate-Kenny_Beltrey.ogg';

var od = new ogg.Decoder();
od.on('stream', function (stream) {
  var vd = new vorbis.Decoder();

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

  stream.pipe(vd);
});

fs.createReadStream(file).pipe(od);
```

Encoder example:

``` javascript
var ogg = require('ogg');
var vorbis = require('vorbis');

var oe = new ogg.Encoder();
var ve = new vorbis.Encoder();

// not yet implemented...
ve.addComment('ARTIST', 'Bob Marley');

// `process.stdin` *MUST* be PCM float 32-bit signed little-endian samples.
// channels and sample rate are configurable but default to 2 and 44,100hz.
process.stdin.pipe(ve)

// send the encoded Vorbis pages to the Ogg encoder
ve.pipe(oe.stream());

// write the produced Ogg file with Vorbis audio to `process.stdout`
oe.pipe(process.stdout);
```

See the `examples` directory for some more example code.

API
---

### Decoder class


### Encoder class
