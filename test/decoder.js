
/**
 * Module dependencies.
 */

var fs = require('fs');
var ogg = require('ogg');
var path = require('path');
var vorbis = require('../');
var assert = require('assert');
var fixtures = path.resolve(__dirname, 'fixtures');

describe('Decoder', function () {

  describe('Rondo_Alla_Turka.ogg', function () {
    var fixture = path.resolve(fixtures, 'Rondo_Alla_Turka.ogg');

    it('should emit at least one "readable" event', function (done) {
      var od = new ogg.Decoder();
      od.on('stream', function (stream) {
        var vd = new vorbis.Decoder(stream);
        vd.on('readable', done);

        // apparently this is what we have to do to make "readable" happen...
        vd.read(0);
      });
      fs.createReadStream(fixture).pipe(od);
    });

    it('should emit an "end" event', function (done) {
      this.test.slow(5000);
      this.test.timeout(10000);

      var od = new ogg.Decoder();
      od.on('stream', function (stream) {
        var vd = new vorbis.Decoder(stream);
        vd.on('end', done);

        // flow...
        vd.resume();
      });
      fs.createReadStream(fixture).pipe(od);
    });

  });

});
