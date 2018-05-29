
/**
 * Module dependencies.
 */

var debug = require('debug')('vorbis:decoder');
var binding = require('./binding');
var inherits = require('util').inherits;
var Transform = require('readable-stream/transform');
var bufferAlloc = require('buffer-alloc');

/**
 * Module exports.
 */

module.exports = Decoder;

/**
 * The Vorbis `Decoder` class.
 * Accepts `ogg_packet` Buffer instances and outputs PCM audio data.
 *
 * @param {Object} opts
 * @api public
 */

function Decoder (opts) {
  if (!(this instanceof Decoder)) return new Decoder(opts);
  Transform.call(this, opts);

  // XXX: nasty hack since we can't set only the Readable props through the
  //      Transform constructor.
  // the writable side (the input end) should accept regular Objects
  this._writableState.objectMode = true;
  this._writableState.lowWaterMark = 0;
  this._writableState.highWaterMark = 0;

  // headerin() needs to be called 3 times
  this._headerCount = 3;

  this.vi = bufferAlloc(binding.sizeof_vorbis_info);
  this.vc = bufferAlloc(binding.sizeof_vorbis_comment);
  binding.vorbis_info_init(this.vi);
  binding.vorbis_comment_init(this.vc);

  // the `vorbis_dsp_state` and `vorbis_block` stucts get allocated after the
  // headers have been parsed
  this.vd = null;
  this.vb = null;
}
inherits(Decoder, Transform);

/**
 * Alias `packetin()` as `write()`, for backwards-compat.
 */

Decoder.prototype.packetin = Decoder.prototype.write;

/**
 * Called for the stream that's being decoded's "packet" event.
 * This function passes the "ogg_packet" struct to the libvorbis backend.
 *
 * @api private
 */

Decoder.prototype._transform = function (packet, _, cb) {
  debug('_transform()');

  var r;
  var self = this;
  if (this._headerCount > 0) {
    debug('headerin', this._headerCount);
    // still decoding the header...
    var vi = this.vi;
    var vc = this.vc;
    binding.vorbis_synthesis_headerin(vi, vc, packet, function (r) {
      debug('headerin return = %d', r);
      if (r !== 0) {
        cb(new Error('headerin() failed: ' + r));
        return;
      }
      this._headerCount--;
      if (!this._headerCount) {
        debug('done parsing Vorbis header');
        var comments = binding.comment_array(vc);
        this.comments = comments;
        this.vendor = comments.vendor;
        this.emit('comments', comments);

        var format = binding.get_format(vi);
        for (r in format) {
          this[r] = format[r];
        }
        this.emit('format', format);
        var err = this._synthesis_init();
        if (err) return cb(err);
      }
      cb();
    }.bind(this));
  } else {
    debug('synthesising ogg_packet (packetno %d)', packet.packetno);
    var vd = this.vd;
    var vb = this.vb;
    var channels = this.channels;
    var eos = !!packet.e_o_s;
    if (eos) debug('got "eos" packet');

    // TODO: async...
    r = binding.vorbis_synthesis(vb, packet);
    if (r !== 0) {
      return cb(new Error('vorbis_synthesis() failed: ' + r));
    }

    // TODO: async...
    r = binding.vorbis_synthesis_blockin(vd, vb);
    if (r !== 0) {
      return cb(new Error('vorbis_synthesis_blockin() failed: ' + r));
    }

    pcmout();
  }

  function pcmout () {
    // TODO: async...
    var b = binding.vorbis_synthesis_pcmout(vd, channels);
    if (b === 0) {
      debug('need more "vorbis_block" data...');
      if (eos) self.push(null); // emit "end"
      cb();
    } else if (b < 0) {
      // some other error...
      cb(new Error('vorbis_synthesis_pcmout() failed: ' + b));
    } else {
      debug('got PCM data (%d bytes)', b.length);

      self.push(b);

      // try to get more data out
      pcmout();
    }
  }
};

/**
 * Called once the 3 Vorbis header packets have been parsed.
 * Allocates `vorbis_dsp_state` and `vorbis_block` structs.
 * Then calls `vorbis_synthesis_init()` and `vorbis_block_init()`.
 *
 * @api private
 */

Decoder.prototype._synthesis_init = function () {
  debug('_synthesis_init()');
  this.vd = bufferAlloc(binding.sizeof_vorbis_dsp_state);
  this.vb = bufferAlloc(binding.sizeof_vorbis_block);
  var r = binding.vorbis_synthesis_init(this.vd, this.vi);
  if (r !== 0) {
    return new Error(r);
  }
  r = binding.vorbis_block_init(this.vd, this.vb);
  if (r !== 0) {
    return new Error(r);
  }
};
