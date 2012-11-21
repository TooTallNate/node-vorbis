
/**
 * We have to load node-ogg in order for the libogg symbols
 * to be visible on Windows.
 */

require('ogg');

/**
 * The `Decoder` class. Write `ogg_packet`s to it and it will output
 * raw PCM float data.
 */

exports.Decoder = require('./lib/decoder');

/**
 * The `Encoder` class.
 */

//exports.Encoder = require('./lib/encoder');
