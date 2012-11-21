
/**
 * We have to load node-ogg in order for the libogg symbols
 * to be visible on Windows.
 */

require('ogg');

exports.Decoder = require('./lib/decoder');

//exports.Encoder = require('./lib/encoder');
