
/**
 * Module dependencies.
 */

var debug = require('debug')('vorbis');
var binding = require('./binding');
var inherits = require('util').inherits;
var Readable = require('stream').Readable;

// node v0.8.x compat
if (!Readable) Readable = require('readable-stream');

/**
 * Module exports.
 */

module.exports = Decoder;

/**
 * The `Decoder` class.
 * Accepts "ogg_page" Buffer instances and outputs raw PCM data.
 *
 * @param {Object} opts
 * @api public
 */

function Decoder (stream, opts) {
  if (!(this instanceof Decoder)) return new Decoder(stream, opts);
  Readable.call(this, opts);

  // the "ogg stream" to decode
  this._stream = stream;

  // headerin() needs to be called 3 times
  this._headerCount = 3;

  this.vi = new Buffer(binding.sizeof_vorbis_info);
  this.vc = new Buffer(binding.sizeof_vorbis_comment);
  binding.vorbis_info_init(this.vi);
  binding.vorbis_comment_init(this.vc);

  stream.on('packet', this._onPacket.bind(this));
}
inherits(Decoder, Readable);

/**
 * Called for the stream that's being decoded's "packet" event.
 * This function passes the "ogg_packet" struct to the libvorbis backend.
 */

Decoder.prototype._onPacket = function (packet, done) {
  debug('_onPacket()');
  if (this._headerCount > 0) {
    debug('headerin', this._headerCount);
    // still decoding the header...
    // TODO: async
    var vi = this.vi;
    var vc = this.vc;
    var r = binding.vorbis_synthesis_headerin(vi, vc, packet);
    if (0 !== r) {
      this.emit('error', new Error('headerin failed: ' + r));
      return;
    }
    this._headerCount--;
    if (!this._headerCount) {
      this.emit('comments', binding.comment_array(vc));
      this.emit('format', binding.get_format(vi));
    }
    process.nextTick(done);
  } else {
    debug('decoding regular...');
    throw new Error('implement!');

  }
};

/**
 *
 */

Decoder.prototype._read = function (bytes, done) {
  debug('_read(%d bytes)', bytes);

};
