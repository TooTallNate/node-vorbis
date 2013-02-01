
/**
 * node-ogg must be loaded first in order for the
 * libogg symbols to be visible on Windows.
 */

require('ogg');

/**
 * The `Decoder` class. Write `ogg_packet`s to it and it will output
 * raw PCM float data.
 */

exports.Decoder = require('./lib/decoder');

/**
 * The `Encoder` class. Write raw PCM float data to it and it'll produce
 * `ogg_packet`s that you can weld into an ogg.Encoder stream.
 */

exports.Encoder = require('./lib/encoder');
