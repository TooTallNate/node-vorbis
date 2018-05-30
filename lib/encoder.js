
/**
 * Module dependencies.
 */

var os = require('os');
var binding = require('./binding');
var inherits = require('util').inherits;
var Transform = require('readable-stream/transform');
var OGGPacket = require('ogg').ogg_packet;
var debug = require('debug')('vorbis:encoder');
var bufferAlloc = require('buffer-alloc');

// determine the native host endianness, the only supported encoding endianness
// assume little-endian for older versions of node.js
var endianness = (typeof os.endianness === 'function') ? os.endianness() : 'LE';

function noop (_) {}

/**
 * Module exports.
 */

module.exports = Encoder;

/**
 * The Vorbis `Encoder` class.
 * Accepts PCM audio data and outputs `OGGPacket` Buffer instances.
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
  if (!opts) opts = {};
  Transform.call(this, opts);

  // the readable side (the output end) should output regular objects
  this._readableState.objectMode = true;
  this._readableState.lowWaterMark = 0;
  this._readableState.highWaterMark = 0;

  // set to `true` after the headerout() call
  this._headerWritten = false;

  // range from -0.1 to 1.0
  this.quality = (opts.quality == null) ? 0.6 : +opts.quality;
  if (this.quality < -0.1 || this.quality > 1.0) {
    throw new Error('"quality" must be in the range -0.1...1.0, got ' + this.quality);
  }

  // set default PCM formatting options
  this._format({
    channels: 2,
    sampleRate: 44100,
    float: true,
    endianness: endianness,
    signed: true,
    bitDepth: 32
  });
  // set specific PCM formatting options
  this._format(opts);
  this.on('pipe', this._pipe);

  this.vi = bufferAlloc(binding.sizeof_vorbis_info);
  this.vc = bufferAlloc(binding.sizeof_vorbis_comment);
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
 * @param {String} contents value (i.e. "my awesome script")
 * @api public
 */

Encoder.prototype.addComment = function (tag, contents) {
  if (this._headerWritten) {
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

Encoder.prototype._transform = function (chunk, _, cb) {
  debug('_transform(%d bytes)', chunk.length);

  // ensure the vorbis header has been output first
  var self = this;
  if (this._headerWritten) {
    process();
  } else {
    this._writeHeader(process);
  }

  // time to write the PCM buffer to the vorbis encoder
  function process (err) {
    if (err) return cb(err);
    self._writepcm(chunk, written);
  }

  // after the PCM buffer has been written, read out the encoded "block"s
  function written (err) {
    if (err) return cb(err);
    self._blockout(cb);
  }
};

/**
 * Initializes the "analysis" data structures and creates the first 3 Vorbis
 * packets to be written to the output ogg stream.
 *
 * @api private
 */

Encoder.prototype._writeHeader = function (cb) {
  var r;

  debug('_writeHeader()');

  // encoder init (only VBR currently supported)
  var channels = this.channels;
  var sampleRate = this.sampleRate;
  // TODO: async maybe?
  r = binding.vorbis_encode_init_vbr(this.vi, channels, sampleRate, this.quality);
  debug('vorbis_encode_init_vbr() return = %d', r);
  if (r !== 0) return cb(new Error(r));

  // synthesis init
  this.vd = bufferAlloc(binding.sizeof_vorbis_dsp_state);
  this.vb = bufferAlloc(binding.sizeof_vorbis_block);
  r = binding.vorbis_analysis_init(this.vd, this.vi);
  debug('vorbis_analysis_init() return = %d', r);
  if (r !== 0) return cb(new Error(r));
  r = binding.vorbis_block_init(this.vd, this.vb);
  debug('vorbis_block_init() return = %d', r);
  if (r !== 0) return cb(new Error(r));

  // create the first 3 header packets
  // TODO: async
  var opHeader = new OGGPacket();
  var opComments = new OGGPacket();
  var opCode = new OGGPacket();
  r = binding.vorbis_analysis_headerout(this.vd, this.vc, opHeader, opComments, opCode);
  debug('vorbis_analysis_headerout() return = %d', r);
  if (r !== 0) return cb(new Error(r));

  // libvorbis will modify the backing buffers for these `OGGPacket` instances
  // as soon as we write some PCM data to the encoder, therefore we must copy the
  // "packet" contents over the node.js Buffer instances so that we have full
  // control over the bytes until whenever the GC cleans them up.
  opHeader.replace();
  opComments.replace();
  opCode.replace();

  this.push(opHeader); // automatically gets placed in its own `ogg_page`
  this.push(opComments);

  // specify that a page flush() call is required after this 3rd packet
  opCode.flush = true;
  this.push(opCode);

  // don't call this function again
  this._headerWritten = true;

  process.nextTick(cb);
};

/**
 * Writes the given Buffer `buf` to the vorbis backend encoder.
 *
 * @api private
 */

Encoder.prototype._writepcm = function (buf, cb) {
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

    noop(buf); // keep ref to "buf" for the async call...

    if (rtn !== 0) {
      // error code
      return cb(new Error('vorbis_analysis_write() error: ' + rtn));
    }

    // success
    cb();
  });
};

/**
 * Calls `vorbis_analysis_blockout()` continuously until no more blocks are
 * returned. For each "block" that gets returned, _flushpacket() is called to
 * extract any possible `OGGPacket` instances from the block.
 *
 * @api private
 */

Encoder.prototype._blockout = function (cb) {
  debug('_blockout');
  var vd = this.vd;
  var vb = this.vb;
  var self = this;
  binding.vorbis_analysis_blockout(vd, vb, function (rtn) {
    debug('vorbis_analysis_blockout() return = %d', rtn);
    if (rtn === 1) {
      // got a "block"

      // analysis, assume we want to use bitrate management
      // TODO: async?
      // TODO: check return values
      binding.vorbis_analysis(vb, null);
      // console.error('vorbis_analysis() = %d', r);
      binding.vorbis_bitrate_addblock(vb);
      // console.error('vorbis_bitrate_addblock() = %d', r);

      self._flushpacket(afterFlush);
    } else if (rtn === 0) {
      // need more PCM data...
      cb();
    } else {
      // error code
      cb(new Error('vorbis_analysis_blockout() error: ' + rtn));
    }
  });
  function afterFlush (err) {
    if (err) return cb(err);
    // now attempt to read another "block"
    self._blockout(cb);
  }
};

/**
 * Calls `vorbis_bitrate_flushpacket()` continuously until no more `OGGPacket`s
 * are returned.
 *
 * @api private
 */

Encoder.prototype._flushpacket = function (cb) {
  debug('_flushpacket()');
  var self = this;
  var packet = new OGGPacket();
  binding.vorbis_bitrate_flushpacket(this.vd, packet, function (rtn) {
    debug('vorbis_bitrate_flushpacket() return = %d', rtn);
    if (rtn === 1) {
      packet.replace();

      // got a packet, output it
      // the consumer should call `pageout()` after this packet
      packet.pageout = true;
      self.push(packet);

      // attempt to get another `OGGPacket`...
      self._flushpacket(cb);
    } else if (rtn === 0) {
      // need more "block" data
      cb();
    } else {
      // error code
      cb(new Error('vorbis_bitrate_flushpacket() error: ' + rtn));
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

Encoder.prototype._flush = function (cb) {
  debug('_onflush()');

  // ensure the vorbis header has been output first
  if (this._headerWritten) {
    process.call(this);
  } else {
    this._writeHeader(process);
  }

  function process () {
    var r = binding.vorbis_analysis_eos(this.vd, 0);
    if (r === 0) {
      this._blockout(cb);
    } else {
      // error code
      cb(new Error('vorbis_analysis_eos() error: ' + r));
    }
  }
};

/**
 * Set given PCM formatting options. Called during instantiation on the passed in
 * options object, on the stream given to the "pipe" event, and a final time if
 * that stream emits a "format" event.
 *
 * @param {Object} opts
 * @api private
 */

Encoder.prototype._format = function (opts) {
  debug('format(keys = %j)', Object.keys(opts));

  // channels is configurable
  if (opts.channels != null) {
    debug('setting "channels"', opts.channels);
    this.channels = opts.channels;
  }

  // the sample rate is configurable
  if (opts.sampleRate != null) {
    debug('setting "sampleRate"', opts.sampleRate);
    this.sampleRate = opts.sampleRate;
  }

  // only signed 32-bit float samples are supported
  if (opts.bitDepth != null) {
    if (opts.bitDepth === 32) {
      debug('setting "bitDepth"', opts.bitDepth);
      this.bitDepth = opts.bitDepth;
    } else {
      return this.emit('error', new Error('only signed `32-bit` float samples are supported, got "' + opts.bitDepth + '"'));
    }
  }

  // only signed 32-bit float samples are supported
  if (opts.float != null) {
    if (opts.float) {
      debug('setting "float"', opts.float);
      this.float = opts.float;
    } else {
      return this.emit('error', new Error('only signed 32-bit `float` samples are supported, got "' + opts.float + '"'));
    }
  }

  // only signed 32-bit float samples are supported
  if (opts.signed != null) {
    if (opts.signed) {
      debug('setting "signed"', opts.signed);
      this.signed = opts.signed;
    } else {
      return this.emit('error', new Error('only `signed` 32-bit float samples are supported, got "' + opts.signed + '"'));
    }
  }

  // only native endianness is supported
  if (opts.endianness != null) {
    if (opts.endianness === endianness) {
      debug('setting "endianness"', endianness);
      this.endianness = endianness;
    } else {
      return this.emit('error', new Error('only native endianness ("' + endianness + '") is supported, got "' + opts.endianness + '"'));
    }
  }
};

/**
 * Called when this stream is pipe()d to from another readable stream.
 * If the "sampleRate", "channels", "bitDepth", "signed", etc. properties are
 * set, then they will be used over the currently set values.
 *
 * @api private
 */

Encoder.prototype._pipe = function (source) {
  debug('_pipe()');
  this._format(source);
  source.once('format', this._format.bind(this));
};
