
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

  describe('pipershut_lo.ogg', function () {
    var fixture = path.resolve(fixtures, 'pipershut_lo.ogg');

    it('should emit at least one "readable" event', function (done) {
      var od = new ogg.Decoder();
      od.on('stream', function (stream) {
        var vd = new vorbis.Decoder();
        vd.on('readable', done);
        stream.pipe(vd);

        // apparently this is what we have to do to make "readable" happen...
        vd.read(0);
      });
      fs.createReadStream(fixture).pipe(od);
    });

    it('should emit a "comments" event', function (done) {
      //this.test.slow(8000);
      //this.test.timeout(10000);

      var od = new ogg.Decoder();
      od.on('stream', function (stream) {
        var vd = new vorbis.Decoder();
        vd.on('comments', function (comments) {
          assert(Array.isArray(comments));
          assert.equal(comments.vendor, 'Lavf54.59.106');
          assert.equal(8, comments.length);
          assert.equal('TRACKNUMBER=8', comments[0]);
          assert.equal('copyright=the Cuffe Family', comments[1]);
          assert.equal('genre=Celtic', comments[2]);
          assert.equal('album=Sae Will We Yet', comments[3]);
          assert.equal('artist=Tony Cuffe & Billy Jackson', comments[4]);
          assert.equal('title=The Burning of the Piper\'s Hut', comments[5]);
          assert.equal('date=2003', comments[6]);
          assert.equal('encoder=Lavf54.59.106', comments[7]);
          done();
        });
        stream.pipe(vd);

        // flow...
        vd.resume();
      });
      fs.createReadStream(fixture).pipe(od);
    });

    it('should emit an "end" event', function (done) {
      this.test.slow(8000);
      this.test.timeout(10000);

      var od = new ogg.Decoder();
      od.on('stream', function (stream) {
        var vd = new vorbis.Decoder();
        vd.on('end', done);
        stream.pipe(vd);

        // flow...
        vd.resume();
      });
      fs.createReadStream(fixture).pipe(od);
    });

  });

  describe('Rooster_crowing_small.ogg', function () {
    var fixture = path.resolve(fixtures, 'Rooster_crowing_small.ogg');

    it('should emit 1 vorbis stream out of 3 ogg streams', function (done) {
      var od = new ogg.Decoder();
      var streamCount = 0;
      var vorbisCount = 0;
      od.on('stream', function (stream) {
        streamCount++;
        // read the first "packet"
        read();
        function read () {
          var packet = stream.read();
          if (packet) {
            vorbis.isVorbis(packet, function (err, is) {
              if (err) return done(err);
              if (is) vorbisCount++;
              stream.resume();
            });
          } else {
            // need to wait
            stream.once('readable', read);
          }
        }
      });
      od.on('finish', function () {
        assert.equal(3, streamCount);
        assert.equal(1, vorbisCount);
        done();
      });
      fs.createReadStream(fixture).pipe(od);
    });

  });

});
