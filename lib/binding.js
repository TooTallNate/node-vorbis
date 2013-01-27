
/**
 * node-ogg must be loaded first in order for the
 * libogg symbols to be visible on Windows.
 */

require('ogg');

/**
 * Module exports.
 */

module.exports = require('bindings')('vorbis');
