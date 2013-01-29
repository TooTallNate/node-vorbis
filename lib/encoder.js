
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

  // the readable side (the output end) should output regular objects
  this._readableState.objectMode = true;
  this._readableState.lowWaterMark = 0;
  this._readableState.highWaterMark = 0;

  // set to `true` after the headerout() call
  this._headerWritten = false;

  // range from -0.1 to 1.0
  this._quality = 0.6;

  // PCM formatting options
  this.channels = 2;
  this.sampleRate = 44100;
  this.float = true;
  this.signed = true;
  this.bitDepth = 32; // sizeof(float) * 8

  this.vi = new Buffer(binding.sizeof_vorbis_info);
  this.vc = new Buffer(binding.sizeof_vorbis_comment);
  binding.vorbis_info_init(this.vi);
  binding.vorbis_comment_init(this.vc);

  // the `vorbis_dsp_state` and `vorbis_block` stucts get allocated when the
  // initial 3 header packets are being written
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

Encoder.prototype._transform = function (buf, output, fn) {
  debug('_transform(%d bytes)', buf.length);

  // ensure the vorbis header has been output first
  var self = this;
  if (this._headerWritten) {
    process();
  } else {
    this._writeHeader(output, process);
  }

  // time to write the PCM buffer to the vorbis encoder
  function process (err) {
    if (err) return fn(err);
    self._writepcm(buf, written);
  }

  // after the PCM buffer has been written, read out the encoded "block"s
  function written (err) {
    if (err) return fn(err);
    self._blockout(output, fn);
  }
};

/**
 * Initializes the "analysis" data structures and creates the first 3 Vorbis
 * packets to be written to the output ogg stream.
 *
 * @api private
 */

Encoder.prototype._writeHeader = function (output, fn) {
  debug('_writeHeader()');

  // encoder init (only VBR currently supported)
  var channels = this.channels;
  var sampleRate = this.sampleRate;
  // TODO: async maybe?
  r = binding.vorbis_encode_init_vbr(this.vi, channels, sampleRate, this._quality);
  debug('vorbis_encode_init_vbr() return = %d', r);
  if (0 !== r) return fn(new Error(r));

  // synthesis init
  this.vd = new Buffer(binding.sizeof_vorbis_dsp_state);
  this.vb = new Buffer(binding.sizeof_vorbis_block);
  var r = binding.vorbis_analysis_init(this.vd, this.vi);
  debug('vorbis_analysis_init() return = %d', r);
  if (0 !== r) return fn(new Error(r));
  r = binding.vorbis_block_init(this.vd, this.vb);
  debug('vorbis_block_init() return = %d', r);
  if (0 !== r) return fn(new Error(r));

  // create first 3 packets
  // TODO: async
  var op_header = new Buffer(binding.sizeof_ogg_packet);
  var op_comments = new Buffer(binding.sizeof_ogg_packet);
  var op_code = new Buffer(binding.sizeof_ogg_packet);
  r = binding.vorbis_analysis_headerout(this.vd, this.vc, op_header, op_comments, op_code);
  debug('vorbis_analysis_headerout() return = %d', r);
  if (0 !== r) return fn(new Error(r));

  output(op_header); // automatically gets placed in its own page
  output(op_comments);
  // specify that a page flush() call is required after this packet
  op_code.flush = true;
  output(op_code);

  // don't call this function again
  this._headerWritten = true;

  process.nextTick(fn);
};

/**
 * Writes the given Buffer `buf` to the vorbis backend encoder.
 *
 * @api private
 */

Encoder.prototype._writepcm = function (buf, fn) {
  debug('_writepcm(%d bytes)', buf.length);

  var channels = this.channels;
  var blockAlign = this.bitDepth / 8 * channels;
  var samples = buf.length / blockAlign | 0;
  var leftover = (samples * blockAlign) - buf.length;
  if (leftover > 0) {
    console.error('%d bytes leftover!', leftover);
    throw new Error('implement "leftover"!');
  }

  binding.vorbis_analysis_write(this.vd, buf, channels, samples, function (rtn) {
    debug('vorbis_analysis_write() return = %d', rtn);

    buf = buf; // keep ref to "buf" for the async call...

    if (0 === rtn) {
      // success
      fn();
    } else {
      // error code
      fn(new Error('vorbis_analysis_write() error: ' + rtn));
    }
  });
};

/**
 * Calls `vorbis_analysis_blockout()` continuously until no more blocks are
 * returned. For each "block" that gets returned, _flushpacket() is called to
 * extract any possible `ogg_packet` instances from the block.
 *
 * @api private
 */

Encoder.prototype._blockout = function (output, fn) {
  debug('_blockout');
  var vd = this.vd;
  var vb = this.vb;
  var self = this;
  binding.vorbis_analysis_blockout(vd, vb, function (rtn) {
    if (1 === rtn) {
      // got a "block"

      // analysis, assume we want to use bitrate management
      // TODO: async?
      // TODO: check return values
      binding.vorbis_analysis(vb, null);
      binding.vorbis_bitrate_addblock(vb);

      self._flushpacket(output, afterFlush);
    } else if (0 === rtn) {
      // need more PCM data...
      fn();
    } else {
      // error code
      fn(new Error('vorbis_analysis_blockout() error: ' + rtn));
    }
  });
  function afterFlush (err) {
    if (err) return fn(err);
    // now attempt to read another "block"
    self._blockout(output, fn);
  }
};

/**
 * Calls `vorbis_bitrate_flushpacket()` continuously until no more `ogg_packet`s
 * are returned.
 *
 * @api private
 */

Encoder.prototype._flushpacket = function (output, fn) {
  debug('_flushpacket()');
  var self = this;
  var packet = new Buffer(binding.sizeof_ogg_packet);
  binding.vorbis_bitrate_flushpacket(this.vd, packet, function (rtn) {
    debug('vorbis_bitrate_flushpacket() return = %d', rtn);
    if (1 === rtn) {
      // got a packet, output it
      // the consumer should call `pageout()` after this packet
      packet.pageout = true;
      output(packet);

      // attempt to get another `ogg_packet`...
      self._flushpacket(output, fn);
    } else if (0 === rtn) {
      // need more "block" data
      fn();
    } else {
      // error code
      fn(new Error('vorbis_bitrate_flushpacket() error: ' + rtn));
    }
  });
};

/**
 * This function calls the `vorbis_analysis_wrote(this.vd, 0)` function, which
 * implies to libvorbis that the end of the audio PCM stream has been reached,
 * and that it's time to close up the ogg stream.
 *
 * @api private
 */

Encoder.prototype._flush = function (output, fn) {
  debug('_onflush()');

  // ensure the vorbis header has been output first
  if (this._headerWritten) {
    process.call(this);
  } else {
    this._writeHeader(output, process);
  }

  function process () {
    var r = binding.vorbis_analysis_eos(this.vd, 0);
    if (0 === r) {
      this._blockout(output, fn);
    } else {
      // error code
      fn(new Error('vorbis_analysis_eos() error: ' + r));
    }
  }
};
