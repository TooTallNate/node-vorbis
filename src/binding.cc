/*
 * Copyright (c) 2012, Nathan Rajlich <nathan@tootallnate.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * libvorbis API reference:
 * http://xiph.org/vorbis/doc/libvorbis/reference.html
 */

#include <v8.h>
#include <node.h>
#include <nan.h>
#include <string.h>

#include "node_buffer.h"
#include "node_pointer.h"
#include "ogg/ogg.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisenc.h"

using namespace v8;
using namespace node;

namespace nodevorbis {


NAN_METHOD(node_vorbis_info_init) {
  NanScope();
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(args[0]);
  vorbis_info_init(vi);
  NanReturnUndefined();
}


NAN_METHOD(node_vorbis_comment_init) {
  NanScope();
  vorbis_comment *vc = UnwrapPointer<vorbis_comment *>(args[0]);
  vorbis_comment_init(vc);
  NanReturnUndefined();
}


NAN_METHOD(node_vorbis_synthesis_init) {
  NanScope();
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(args[1]);
  int r = vorbis_synthesis_init(vd, vi);
  NanReturnValue(NanNew<Integer>(r));
}


NAN_METHOD(node_vorbis_analysis_init) {
  NanScope();
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(args[1]);
  int r = vorbis_analysis_init(vd, vi);
  NanReturnValue(NanNew<Integer>(r));
}


NAN_METHOD(node_vorbis_block_init) {
  NanScope();
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(args[1]);
  int r = vorbis_block_init(vd, vb);
  NanReturnValue(NanNew<Integer>(r));
}


NAN_METHOD(node_vorbis_encode_init_vbr) {
  NanScope();
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(args[0]);
  long channels = args[1]->IntegerValue();
  long rate = args[2]->IntegerValue();
  float quality = args[3]->NumberValue();
  int r = vorbis_encode_init_vbr(vi, channels, rate, quality);
  NanReturnValue(NanNew<Integer>(r));
}


NAN_METHOD(node_comment_array) {
  NanScope();
  int i;
  vorbis_comment *vc = UnwrapPointer<vorbis_comment *>(args[0]);
  Local<Array> array = NanNew<Array>(vc->comments);
  for (i = 0; i < vc->comments; i++) {
    array->Set(i, NanNew<String>(vc->user_comments[i], vc->comment_lengths[i]));
  }
  array->Set(NanNew<String>("vendor"), NanNew<String>(vc->vendor));
  NanReturnValue(array);
}


NAN_METHOD(node_get_format) {
  NanScope();
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(args[0]);
  Local<Object> format = NanNew<Object>();

  /* PCM format properties */
  format->Set(NanNew<String>("channels"), NanNew<Integer>(vi->channels));
  format->Set(NanNew<String>("sampleRate"), NanNew<Number>(vi->rate));
  format->Set(NanNew<String>("bitDepth"), NanNew<Integer>(static_cast<int32_t>(sizeof(float) * 8)));
  format->Set(NanNew<String>("float"), NanTrue());
  format->Set(NanNew<String>("signed"), NanTrue());

  /* Other random info from the vorbis_info struct */
  format->Set(NanNew<String>("version"), NanNew<Integer>(vi->version));
  format->Set(NanNew<String>("bitrateUpper"), NanNew<Number>(vi->bitrate_upper));
  format->Set(NanNew<String>("bitrateNominal"), NanNew<Number>(vi->bitrate_nominal));
  format->Set(NanNew<String>("bitrateLower"), NanNew<Number>(vi->bitrate_lower));
  format->Set(NanNew<String>("bitrateWindow"), NanNew<Number>(vi->bitrate_window));

  NanReturnValue(format);
}


/* TODO: async */
NAN_METHOD(node_vorbis_analysis_headerout) {
  NanScope();
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  vorbis_comment *vc = UnwrapPointer<vorbis_comment *>(args[1]);
  ogg_packet *op_header = UnwrapPointer<ogg_packet *>(args[2]);
  ogg_packet *op_comments = UnwrapPointer<ogg_packet *>(args[3]);
  ogg_packet *op_code = UnwrapPointer<ogg_packet *>(args[4]);
  int r = vorbis_analysis_headerout(vd, vc, op_header, op_comments, op_code);
  NanReturnValue(NanNew<Integer>(r));
}


/* combination of `vorbis_analysis_buffer()`, `memcpy()`, and
 * `vorbis_analysis_wrote()` on the thread pool. */

class AnalysisWriteWorker : public NanAsyncWorker {
 public:
  AnalysisWriteWorker(vorbis_dsp_state *vd, float *buffer, int channels, long samples, NanCallback *callback)
    : vd(vd), buffer(buffer), channels(channels), samples(samples), rtn(0),
    NanAsyncWorker(callback) { }
  ~AnalysisWriteWorker() { }
  void Execute () {
    /* input samples are interleaved floats */
    int i = 0, j = 0;

    /* expose buffer to write PCM float samples to */
    float **output = vorbis_analysis_buffer(vd, samples);

    /* uninterleave samples */
    for (i = 0; i < samples; i++) {
      for (j = 0; j < channels; j++) {
        output[j][i] = buffer[i * channels + j];
      }
    }

    /* tell the library how much we actually submitted */
    rtn = vorbis_analysis_wrote(vd, i);
  }
  void HandleOKCallback () {
    NanScope();

    Handle<Value> argv[1] = { NanNew<Integer>(rtn) };

    callback->Call(1, argv);
  }
 private:
  vorbis_dsp_state *vd;
  float *buffer;
  int channels;
  long samples;
  int rtn;
};

NAN_METHOD(node_vorbis_analysis_write) {
  NanScope();

  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  float *buffer = UnwrapPointer<float *>(args[1]);
  int channels = args[2]->IntegerValue();
  long samples = args[3]->NumberValue();
  NanCallback *callback = new NanCallback(args[4].As<Function>());

  NanAsyncQueueWorker(new AnalysisWriteWorker(vd, buffer, channels, samples, callback));
  NanReturnUndefined();
}

/* vorbis_analysis_blockout() on the thread pool */
class AnalysisBlockoutWorker : public NanAsyncWorker {
 public:
  AnalysisBlockoutWorker(vorbis_dsp_state *vd, vorbis_block *vb, NanCallback *callback)
    : vd(vd), vb(vb), rtn(0), NanAsyncWorker(callback) { }
  ~AnalysisBlockoutWorker() { }
  void Execute () {
    rtn = vorbis_analysis_blockout(vd, vb);
  }
  void HandleOKCallback () {
    NanScope();

    Handle<Value> argv[1] = { NanNew<Integer>(rtn) };

    callback->Call(1, argv);
  }
 private:
  vorbis_dsp_state *vd;
  vorbis_block *vb;
  int rtn;
};

NAN_METHOD(node_vorbis_analysis_blockout) {
  NanScope();

  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(args[1]);
  NanCallback *callback = new NanCallback(args[2].As<Function>());

  NanAsyncQueueWorker(new AnalysisBlockoutWorker(vd, vb, callback));
  NanReturnUndefined();
}

/* TODO: async? */
NAN_METHOD(node_vorbis_analysis_eos) {
  NanScope();
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);

  int i = vorbis_analysis_wrote(vd, 0);
  NanReturnValue(NanNew<Integer>(i));
}

/* TODO: async? */
NAN_METHOD(node_vorbis_analysis) {
  NanScope();
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(args[0]);
  ogg_packet *op = UnwrapPointer<ogg_packet *>(args[1]);

  int i = vorbis_analysis(vb, op);
  NanReturnValue(NanNew<Integer>(i));
}

/* TODO: async? */
NAN_METHOD(node_vorbis_bitrate_addblock) {
  NanScope();
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(args[0]);

  int i = vorbis_bitrate_addblock(vb);
  NanReturnValue(NanNew<Integer>(i));
}


/* vorbis_bitrate_flushpacket() on the thread pool */
class BitrateFlushpacketWorker : public NanAsyncWorker {
 public:
  BitrateFlushpacketWorker(vorbis_dsp_state *vd, ogg_packet *op, NanCallback *callback)
    : vd(vd), op(op), rtn(0), NanAsyncWorker(callback) { }
  ~BitrateFlushpacketWorker() { }
  void Execute () {
    rtn = vorbis_bitrate_flushpacket(vd, op);
  }
  void HandleOKCallback () {
    NanScope();

    Handle<Value> argv[1] = { NanNew<Integer>(rtn) };

    callback->Call(1, argv);
  }
 private:
  vorbis_dsp_state *vd;
  ogg_packet *op;
  int rtn;
};

NAN_METHOD(node_vorbis_bitrate_flushpacket) {
  NanScope();

  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  ogg_packet *op = UnwrapPointer<ogg_packet *>(args[1]);
  NanCallback *callback = new NanCallback(args[2].As<Function>());

  NanAsyncQueueWorker(new BitrateFlushpacketWorker(vd, op, callback));
  NanReturnUndefined();
}


/* vorbis_synthesis_idheader() called on the thread pool */
class SynthesisIdheaderWorker : public NanAsyncWorker {
 public:
  SynthesisIdheaderWorker(ogg_packet *op, NanCallback *callback)
    : op(op), rtn(0), NanAsyncWorker(callback) { }
  ~SynthesisIdheaderWorker() { }
  void Execute () {
    rtn = vorbis_synthesis_idheader(op);
  }
  void HandleOKCallback () {
    NanScope();

    Handle<Value> argv[] = { NanNull(), NanNew<Boolean>(rtn) };

    callback->Call(2, argv);
  }
 private:
  ogg_packet *op;
  int rtn;
};

NAN_METHOD(node_vorbis_synthesis_idheader) {
  NanScope();

  ogg_packet *op = UnwrapPointer<ogg_packet *>(args[0]);
  NanCallback *callback = new NanCallback(args[1].As<Function>());

  NanAsyncQueueWorker(new SynthesisIdheaderWorker(op, callback));
  NanReturnUndefined();
}


/* vorbis_synthesis_headerin() called on the thread pool */
class SynthesisHeaderinWorker : public NanAsyncWorker {
 public:
  SynthesisHeaderinWorker(vorbis_info *vi, vorbis_comment *vc, ogg_packet *op, NanCallback *callback)
    : vi(vi), vc(vc), op(op), rtn(0), NanAsyncWorker(callback) { }
  ~SynthesisHeaderinWorker() { }
  void Execute () {
      rtn = vorbis_synthesis_headerin(vi, vc, op);
  }
  void HandleOKCallback () {
    NanScope();

    Handle<Value> argv[1] = { NanNew<Integer>(rtn) };

    callback->Call(1, argv);
  }
 private:
  vorbis_info *vi;
  vorbis_comment *vc;
  ogg_packet *op;
  int rtn;
};
NAN_METHOD(node_vorbis_synthesis_headerin) {
  NanScope();

  vorbis_info *vi = UnwrapPointer<vorbis_info *>(args[0]);
  vorbis_comment *vc = UnwrapPointer<vorbis_comment *>(args[1]);
  ogg_packet *op = UnwrapPointer<ogg_packet *>(args[2]);
  NanCallback *callback = new NanCallback(args[3].As<Function>());

  NanAsyncQueueWorker(new SynthesisHeaderinWorker(vi, vc, op, callback));
  NanReturnUndefined();
}



/* TODO: async */
NAN_METHOD(node_vorbis_synthesis) {
  NanScope();
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(args[0]);
  ogg_packet *op = UnwrapPointer<ogg_packet *>(args[1]);

  int i = vorbis_synthesis(vb, op);
  NanReturnValue(NanNew<Integer>(i));
}


/* TODO: async */
NAN_METHOD(node_vorbis_synthesis_blockin) {
  NanScope();
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(args[1]);

  int i = vorbis_synthesis_blockin(vd, vb);
  NanReturnValue(NanNew<Integer>(i));
}


/* TODO: async */
NAN_METHOD(node_vorbis_synthesis_pcmout) {
  NanScope();
  float **pcm;
  int samples;
  Handle<Value> rtn;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  int channels = args[1]->Int32Value();

  samples = vorbis_synthesis_pcmout(vd, &pcm);

  if (samples > 0) {
    /* we need to interlace the pcm float data... */
    Local<Object> buffer = NanNewBufferHandle(samples * channels * sizeof(float));
    float *buf = reinterpret_cast<float *>(Buffer::Data(buffer));
    int i, j;
    for (i = 0; i < channels; i++) {
      float *ptr = buf + i;
      float *mono = pcm[i];
      for (j = 0; j < samples; j++) {
        *ptr = mono[j];
        ptr += channels;
      }
    }
    vorbis_synthesis_read(vd, samples);
    rtn = buffer;
  } else {
    /* an error or 0 samples */
    rtn = NanNew<Integer>(samples);
  }

  NanReturnValue(rtn);
}


void Initialize(Handle<Object> target) {
  NanScope();

  /* sizeof's */
#define SIZEOF(value) \
  target->ForceSet(NanNew<String>("sizeof_" #value), NanNew<Integer>(static_cast<int32_t>(sizeof(value))), \
      static_cast<PropertyAttribute>(ReadOnly|DontDelete))
  SIZEOF(vorbis_info);
  SIZEOF(vorbis_comment);
  SIZEOF(vorbis_dsp_state);
  SIZEOF(vorbis_block);
  SIZEOF(ogg_packet);

  /* contants */
#define CONST(value) \
  target->ForceSet(NanNew<String>(#value), NanNew<Integer>(value), \
      static_cast<PropertyAttribute>(ReadOnly|DontDelete))
  CONST(OV_FALSE);
  CONST(OV_EOF);
  CONST(OV_HOLE);

  CONST(OV_EREAD);
  CONST(OV_EFAULT);
  CONST(OV_EIMPL);
  CONST(OV_EINVAL);
  CONST(OV_ENOTVORBIS);
  CONST(OV_EBADHEADER);
  CONST(OV_EVERSION);
  CONST(OV_ENOTAUDIO);
  CONST(OV_EBADPACKET);
  CONST(OV_EBADLINK);
  CONST(OV_ENOSEEK);

  /* functions */
  NODE_SET_METHOD(target, "vorbis_info_init", node_vorbis_info_init);
  NODE_SET_METHOD(target, "vorbis_comment_init", node_vorbis_comment_init);
  NODE_SET_METHOD(target, "vorbis_synthesis_init", node_vorbis_synthesis_init);
  NODE_SET_METHOD(target, "vorbis_analysis_init", node_vorbis_analysis_init);
  NODE_SET_METHOD(target, "vorbis_block_init", node_vorbis_block_init);
  NODE_SET_METHOD(target, "vorbis_encode_init_vbr", node_vorbis_encode_init_vbr);
  NODE_SET_METHOD(target, "vorbis_analysis_headerout", node_vorbis_analysis_headerout);
  NODE_SET_METHOD(target, "vorbis_analysis_write", node_vorbis_analysis_write);
  NODE_SET_METHOD(target, "vorbis_analysis_blockout", node_vorbis_analysis_blockout);
  NODE_SET_METHOD(target, "vorbis_analysis_eos", node_vorbis_analysis_eos);
  NODE_SET_METHOD(target, "vorbis_analysis", node_vorbis_analysis);
  NODE_SET_METHOD(target, "vorbis_bitrate_addblock", node_vorbis_bitrate_addblock);
  NODE_SET_METHOD(target, "vorbis_bitrate_flushpacket", node_vorbis_bitrate_flushpacket);
  NODE_SET_METHOD(target, "vorbis_synthesis_idheader", node_vorbis_synthesis_idheader);
  NODE_SET_METHOD(target, "vorbis_synthesis_headerin", node_vorbis_synthesis_headerin);
  NODE_SET_METHOD(target, "vorbis_synthesis", node_vorbis_synthesis);
  NODE_SET_METHOD(target, "vorbis_synthesis_blockin", node_vorbis_synthesis_blockin);
  NODE_SET_METHOD(target, "vorbis_synthesis_pcmout", node_vorbis_synthesis_pcmout);

  /* custom functions */
  NODE_SET_METHOD(target, "comment_array", node_comment_array);
  NODE_SET_METHOD(target, "get_format", node_get_format);

  target->ForceSet(NanNew<String>("version"), NanNew<String>(vorbis_version_string()),
    static_cast<PropertyAttribute>(ReadOnly|DontDelete));
}

} // nodevorbis namespace

NODE_MODULE(vorbis, nodevorbis::Initialize);
