
/**
 * Module dependencies.
 */

var binding = require('./lib/binding');

/**
 * libvorbis version string.
 */

exports.version = binding.version;

/**
 * Async function that checks if the given `ogg_packet` is Vorbis data. The packet
 * must be the first packet in the ogg stream.
 */

exports.isVorbis = binding.vorbis_synthesis_idheader;

/**
 * The `Decoder` class. Write `ogg_packet`s to it and it will output
 * raw PCM float data.
 */

exports.Decoder = require('./lib/decoder');

/**
 * The `Encoder` class. Write raw PCM float data to it and it'll produce
 * `ogg_packet`s that you can weld into an ogg.Encoder stream.
 */

//exports.Encoder = require('./lib/encoder');
