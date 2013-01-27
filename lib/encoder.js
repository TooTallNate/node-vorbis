
/**
 * Module dependencies.
 */

var debug = require('debug')('vorbis:encoder');
var binding = require('./binding');
var inherits = require('util').inherits;
var Transform = require('stream').Transform;

// node v0.8.x compat
if (!Transform) Transform = require('readable-stream/transform');

/**
 * Module exports.
 */

module.exports = Encoder;

/**
 * The Vorbis `Encoder` class.
 * Accepts PCM audio data and outputs `ogg_packet` Buffer instances.
 * Input must be 32-bit float samples. You may specify the number of `channels`
 * and the `sampleRate`.
 * You may also specify the "quality" which is a float number from -0.1 to 1.0
 * (low to high quality). If unspecified, the default is 0.6.
 *
 * @param {Object} opts PCM audio format options
 * @api public
 */

function Encoder (opts) {
  if (!(this instanceof Encoder)) return new Encoder(opts);
  Transform.call(this, opts);

  this._writableState.objectMode = true;
  this._writableState.lowWaterMark = 0;
  this._writableState.highWaterMark = 0;

  // set to `true` after the headerout() call
  this._headerWritten = false;

  // range from -0.1 to 1.0
  this._quality = 0.6;

  this.vi = new Buffer(binding.sizeof_vorbis_info);
  this.vc = new Buffer(binding.sizeof_vorbis_comment);
  binding.vorbis_info_init(this.vi);
  binding.vorbis_comment_init(this.vc);

  // the `vorbis_dsp_state` and `vorbis_block` stucts get allocated when the
  // initial 3 packets are being written
  this.vd = null;
  this.vb = null;
}
inherits(Encoder, Transform);

/**
 * Adds a vorbis comment to the output ogg stream.
 * All calls to this function must be made *before* any PCM audio data is written
 * to this encoder.
 *
 * @param {String} tag key name (i.e. "ENCODER")
 * @param {String} content value (i.e. "my awesome script")
 * @api public
 */

Encoder.prototype.addComment = function (tag, contents) {
  if (this.headerWritten) {
    throw new Error('Can\'t add comment since "comment packet" has already been output');
  } else {
    binding.vorbis_comment_add_tag(this.vc, tag, contents);
  }
};

/**
 * Transform stream callback function.
 *
 * @api private
 */

Encoder.prototype._transform = function (packet, output, fn) {
  debug('_transform(%d bytes)', packet.length);
  if (!this._headerWritten) {
    var err = this._writeHeader(output);
    if (err) return fn(err);
  }
};

/**
 * Initializes the "analysis" data structures and creates the first 3 Vorbis
 * packets to be written to the output ogg stream.
 *
 * @api private
 */

Encoder.prototype._writeHeader = function (output) {
  debug('_writeHeader()');

  // encoder init (only VBR currently supported)
  var channels = 2;
  var sampleRate = 44100;
  // TODO: async maybe?
  r = binding.vorbis_encode_init_vbr(this.vi, channels, sampleRate, this._quality);
  if (0 !== r) {
    return new Error(r);
  }

  // synthesis init
  this.vd = new Buffer(binding.sizeof_vorbis_dsp_state);
  this.vb = new Buffer(binding.sizeof_vorbis_block);
  var r = binding.vorbis_analysis_init(this.vd, this.vi);
  if (0 !== r) return new Error(r);
  r = binding.vorbis_block_init(this.vd, this.vb);
  if (0 !== r) return new Error(r);

  // create first 3 packets
  // TODO: async
  var op_header = new Buffer(binding.sizeof_ogg_packet);
  var op_comments = new Buffer(binding.sizeof_ogg_packet);
  var op_code = new Buffer(binding.sizeof_ogg_packet);
  r = binding.vorbis_analysis_headerout(this.vd, this.vc, op_header, op_comments, op_code);
  if (0 !== r) return new Error(r);

  output(op_header); // automatically gets placed in its own page
  output(op_comments);
  output(op_code);

  // imply that a page flush() call is required
  this.emit('flush');

  this._headerWritten = true;
};
