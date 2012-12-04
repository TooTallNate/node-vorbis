
/**
 * Module dependencies.
 */

var inherits = require('util').inherits;

/**
 * Module exports.
 */

exports = module.exports = VorbisError;

/**
 * Map of error names and error messages.
 */

var messages = {
  OV_FALSE: 'Not true, or no data available',
  OV_HOLE: 'Vorbisfile encoutered missing or corrupt data in the bitstream. Recovery is normally automatic and this return code is for informational purposes only.',
  OV_EREAD: 'Read error while fetching compressed data for decode',
  OV_EFAULT: 'Internal inconsistency in encode or decode state. Continuing is likely not possible.',
  OV_EIMPL: 'Feature not implemented',
  OV_EINVAL: 'Either an invalid argument, or incompletely initialized argument passed to a call',
  OV_ENOTVORBIS: 'The given file/data was not recognized as Ogg Vorbis data.',
  OV_EBADHEADER: 'The file/data is apparently an Ogg Vorbis stream, but contains a corrupted or undecipherable header.',
  OV_EVERSION: 'The bitstream format revision of the given stream is not supported.',
  OV_EBADLINK: 'The given link exists in the Vorbis data stream, but is not decipherable due to garbacge or corruption.',
  OV_ENOSEEK: 'The given stream is not seekable'
};

/**
 * Error codes.
 */

var codes = {
  '-1': 'OV_FALSE',
  '-2': 'OV_EOF',
  '-3': 'OV_HOLE',
  '-128': 'OV_EREAD',
  '-129': 'OV_EFAULT',
  '-130': 'OV_EIMPL',
  '-131': 'OV_EINVAL',
  '-132': 'OV_ENOTVORBIS',
  '-133': 'OV_EBADHEADER',
  '-134': 'OV_EVERSION',
  '-135': 'OV_ENOTAUDIO',
  '-136': 'OV_EBADPACKET',
  '-137': 'OV_EBADLINK',
  '-138': 'OV_ENOSEEK'
};

/**
 * The `VorbisError` class.
 */

function VorbisError (code, func) {
  var name = codes[code];
  var message = messages[name];
  if (func) {
    message = func + '(): ' + message;
  }
  Error.call(this);
  Error.captureStackTrace(this, VorbisError);
  this.name = name;
  this.message = message;
}
inherits(VorbisError, Error);
