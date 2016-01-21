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
  Nan::HandleScope scope;
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(info[0]);
  vorbis_info_init(vi);
}


NAN_METHOD(node_vorbis_comment_init) {
  Nan::HandleScope scope;
  vorbis_comment *vc = UnwrapPointer<vorbis_comment *>(info[0]);
  vorbis_comment_init(vc);
}


NAN_METHOD(node_vorbis_synthesis_init) {
  Nan::HandleScope scope;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(info[0]);
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(info[1]);
  int r = vorbis_synthesis_init(vd, vi);
  info.GetReturnValue().Set(Nan::New<Integer>(r));
}


NAN_METHOD(node_vorbis_analysis_init) {
  Nan::HandleScope scope;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(info[0]);
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(info[1]);
  int r = vorbis_analysis_init(vd, vi);
  info.GetReturnValue().Set(Nan::New<Integer>(r));
}


NAN_METHOD(node_vorbis_block_init) {
  Nan::HandleScope scope;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(info[0]);
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(info[1]);
  int r = vorbis_block_init(vd, vb);
  info.GetReturnValue().Set(Nan::New<Integer>(r));
}


NAN_METHOD(node_vorbis_encode_init_vbr) {
  Nan::HandleScope scope;
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(info[0]);
  long channels = info[1]->IntegerValue();
  long rate = info[2]->IntegerValue();
  float quality = info[3]->NumberValue();
  int r = vorbis_encode_init_vbr(vi, channels, rate, quality);
  info.GetReturnValue().Set(Nan::New<Integer>(r));
}


NAN_METHOD(node_comment_array) {
  Nan::HandleScope scope;
  int i;
  vorbis_comment *vc = UnwrapPointer<vorbis_comment *>(info[0]);
  Local<Array> array = Nan::New<Array>(vc->comments);
  for (i = 0; i < vc->comments; i++) {
    Nan::Set(array, i, Nan::New<String>(vc->user_comments[i], vc->comment_lengths[i]).ToLocalChecked());
  }
  Nan::Set(array, Nan::New<String>("vendor").ToLocalChecked(), Nan::New<String>(vc->vendor).ToLocalChecked());
  info.GetReturnValue().Set(array);
}


NAN_METHOD(node_get_format) {
  Nan::HandleScope scope;
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(info[0]);
  Local<Object> format = Nan::New<Object>();

  /* PCM format properties */
  Nan::Set(format, Nan::New<String>("channels").ToLocalChecked() , Nan::New<Integer>(vi->channels));
  Nan::Set(format, Nan::New<String>("sampleRate").ToLocalChecked(), Nan::New<Number>(vi->rate));
  Nan::Set(format, Nan::New<String>("bitDepth").ToLocalChecked(), Nan::New<Integer>(static_cast<int32_t>(sizeof(float) * 8)));
  Nan::Set(format, Nan::New<String>("float").ToLocalChecked(), Nan::True());
  Nan::Set(format, Nan::New<String>("signed").ToLocalChecked(), Nan::True());

  /* Other random info from the vorbis_info struct */
  Nan::Set(format, Nan::New<String>("version").ToLocalChecked(), Nan::New<Integer>(vi->version));
  Nan::Set(format, Nan::New<String>("bitrateUpper").ToLocalChecked(), Nan::New<Number>(vi->bitrate_upper));
  Nan::Set(format, Nan::New<String>("bitrateNominal").ToLocalChecked(), Nan::New<Number>(vi->bitrate_nominal));
  Nan::Set(format, Nan::New<String>("bitrateLower").ToLocalChecked(), Nan::New<Number>(vi->bitrate_lower));
  Nan::Set(format, Nan::New<String>("bitrateWindow").ToLocalChecked(), Nan::New<Number>(vi->bitrate_window));

  info.GetReturnValue().Set(format);
}


/* TODO: async */
NAN_METHOD(node_vorbis_analysis_headerout) {
  Nan::HandleScope scope;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(info[0]);
  vorbis_comment *vc = UnwrapPointer<vorbis_comment *>(info[1]);
  ogg_packet *op_header = UnwrapPointer<ogg_packet *>(info[2]);
  ogg_packet *op_comments = UnwrapPointer<ogg_packet *>(info[3]);
  ogg_packet *op_code = UnwrapPointer<ogg_packet *>(info[4]);
  int r = vorbis_analysis_headerout(vd, vc, op_header, op_comments, op_code);
  info.GetReturnValue().Set(Nan::New<Integer>(r));
}


/* combination of `vorbis_analysis_buffer()`, `memcpy()`, and
 * `vorbis_analysis_wrote()` on the thread pool. */

class AnalysisWriteWorker : public Nan::AsyncWorker {
 public:
  AnalysisWriteWorker(vorbis_dsp_state *vd, float *buffer, int channels, long samples, Nan::Callback *callback)
    : vd(vd), buffer(buffer), channels(channels), samples(samples), rtn(0),
    Nan::AsyncWorker(callback) { }
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
    Nan::HandleScope scope;

    v8::Local<Value> argv[1] = { Nan::New<Integer>(rtn) };

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
  Nan::HandleScope scope;

  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(info[0]);
  float *buffer = UnwrapPointer<float *>(info[1]);
  int channels = info[2]->IntegerValue();
  long samples = info[3]->NumberValue();
  Nan::Callback *callback = new Nan::Callback(info[4].As<Function>());

  Nan::AsyncQueueWorker(new AnalysisWriteWorker(vd, buffer, channels, samples, callback));
}

/* vorbis_analysis_blockout() on the thread pool */
class AnalysisBlockoutWorker : public Nan::AsyncWorker {
 public:
  AnalysisBlockoutWorker(vorbis_dsp_state *vd, vorbis_block *vb, Nan::Callback *callback)
    : vd(vd), vb(vb), rtn(0), Nan::AsyncWorker(callback) { }
  ~AnalysisBlockoutWorker() { }
  void Execute () {
    rtn = vorbis_analysis_blockout(vd, vb);
  }
  void HandleOKCallback () {
    Nan::HandleScope scope;

    v8::Local<Value> argv[1] = { Nan::New<Integer>(rtn) };

    callback->Call(1, argv);
  }
 private:
  vorbis_dsp_state *vd;
  vorbis_block *vb;
  int rtn;
};

NAN_METHOD(node_vorbis_analysis_blockout) {
  Nan::HandleScope scope;

  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(info[0]);
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(info[1]);
  Nan::Callback *callback = new Nan::Callback(info[2].As<Function>());

  Nan::AsyncQueueWorker(new AnalysisBlockoutWorker(vd, vb, callback));
}

/* TODO: async? */
NAN_METHOD(node_vorbis_analysis_eos) {
  Nan::HandleScope scope;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(info[0]);

  int i = vorbis_analysis_wrote(vd, 0);
  info.GetReturnValue().Set(Nan::New<Integer>(i));
}

/* TODO: async? */
NAN_METHOD(node_vorbis_analysis) {
  Nan::HandleScope scope;
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(info[0]);
  ogg_packet *op = UnwrapPointer<ogg_packet *>(info[1]);

  int i = vorbis_analysis(vb, op);
  info.GetReturnValue().Set(Nan::New<Integer>(i));
}

/* TODO: async? */
NAN_METHOD(node_vorbis_bitrate_addblock) {
  Nan::HandleScope scope;
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(info[0]);

  int i = vorbis_bitrate_addblock(vb);
  info.GetReturnValue().Set(Nan::New<Integer>(i));
}


/* vorbis_bitrate_flushpacket() on the thread pool */
class BitrateFlushpacketWorker : public Nan::AsyncWorker {
 public:
  BitrateFlushpacketWorker(vorbis_dsp_state *vd, ogg_packet *op, Nan::Callback *callback)
    : vd(vd), op(op), rtn(0), Nan::AsyncWorker(callback) { }
  ~BitrateFlushpacketWorker() { }
  void Execute () {
    rtn = vorbis_bitrate_flushpacket(vd, op);
  }
  void HandleOKCallback () {
    Nan::HandleScope scope;

    v8::Local<Value> argv[1] = { Nan::New<Integer>(rtn) };

    callback->Call(1, argv);
  }
 private:
  vorbis_dsp_state *vd;
  ogg_packet *op;
  int rtn;
};

NAN_METHOD(node_vorbis_bitrate_flushpacket) {
  Nan::HandleScope scope;

  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(info[0]);
  ogg_packet *op = UnwrapPointer<ogg_packet *>(info[1]);
  Nan::Callback *callback = new Nan::Callback(info[2].As<Function>());

  Nan::AsyncQueueWorker(new BitrateFlushpacketWorker(vd, op, callback));
}


/* vorbis_synthesis_idheader() called on the thread pool */
class SynthesisIdheaderWorker : public Nan::AsyncWorker {
 public:
  SynthesisIdheaderWorker(ogg_packet *op, Nan::Callback *callback)
    : op(op), rtn(0), Nan::AsyncWorker(callback) { }
  ~SynthesisIdheaderWorker() { }
  void Execute () {
    rtn = vorbis_synthesis_idheader(op);
  }
  void HandleOKCallback () {
    Nan::HandleScope scope;

    v8::Local<Value> argv[] = { Nan::Null(), Nan::New<Boolean>(rtn) };

    callback->Call(2, argv);
  }
 private:
  ogg_packet *op;
  int rtn;
};

NAN_METHOD(node_vorbis_synthesis_idheader) {
  Nan::HandleScope scope;

  ogg_packet *op = UnwrapPointer<ogg_packet *>(info[0]);
  Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());

  Nan::AsyncQueueWorker(new SynthesisIdheaderWorker(op, callback));
}


/* vorbis_synthesis_headerin() called on the thread pool */
class SynthesisHeaderinWorker : public Nan::AsyncWorker {
 public:
  SynthesisHeaderinWorker(vorbis_info *vi, vorbis_comment *vc, ogg_packet *op, Nan::Callback *callback)
    : vi(vi), vc(vc), op(op), rtn(0), Nan::AsyncWorker(callback) { }
  ~SynthesisHeaderinWorker() { }
  void Execute () {
      rtn = vorbis_synthesis_headerin(vi, vc, op);
  }
  void HandleOKCallback () {
    Nan::HandleScope scope;

    v8::Local<Value> argv[1] = { Nan::New<Integer>(rtn) };

    callback->Call(1, argv);
  }
 private:
  vorbis_info *vi;
  vorbis_comment *vc;
  ogg_packet *op;
  int rtn;
};
NAN_METHOD(node_vorbis_synthesis_headerin) {
  Nan::HandleScope scope;

  vorbis_info *vi = UnwrapPointer<vorbis_info *>(info[0]);
  vorbis_comment *vc = UnwrapPointer<vorbis_comment *>(info[1]);
  ogg_packet *op = UnwrapPointer<ogg_packet *>(info[2]);
  Nan::Callback *callback = new Nan::Callback(info[3].As<Function>());

  Nan::AsyncQueueWorker(new SynthesisHeaderinWorker(vi, vc, op, callback));
}



/* TODO: async */
NAN_METHOD(node_vorbis_synthesis) {
  Nan::HandleScope scope;
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(info[0]);
  ogg_packet *op = UnwrapPointer<ogg_packet *>(info[1]);

  int i = vorbis_synthesis(vb, op);
  info.GetReturnValue().Set(Nan::New<Integer>(i));
}


/* TODO: async */
NAN_METHOD(node_vorbis_synthesis_blockin) {
  Nan::HandleScope scope;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(info[0]);
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(info[1]);

  int i = vorbis_synthesis_blockin(vd, vb);
  info.GetReturnValue().Set(Nan::New<Integer>(i));
}


/* TODO: async */
NAN_METHOD(node_vorbis_synthesis_pcmout) {
  Nan::HandleScope scope;
  float **pcm;
  int samples;
  v8::Local<Value> rtn;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(info[0]);
  int channels = info[1]->Int32Value();

  samples = vorbis_synthesis_pcmout(vd, &pcm);

  if (samples > 0) {
    /* we need to interlace the pcm float data... */
    Nan::MaybeLocal<Object> buffer = Nan::NewBuffer(samples * channels * sizeof(float));
    float *buf = reinterpret_cast<float *>(Buffer::Data(buffer.ToLocalChecked()));
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
    rtn = buffer.ToLocalChecked();
  } else {
    /* an error or 0 samples */
    rtn = Nan::New<Integer>(samples);
  }

  info.GetReturnValue().Set(rtn);
}


NAN_MODULE_INIT(Initialize) {
  Nan::HandleScope scope;

  /* sizeof's */
#define SIZEOF(value) \
  Nan::ForceSet(target, Nan::New<String>("sizeof_" #value).ToLocalChecked(), Nan::New<Integer>(static_cast<int32_t>(sizeof(value))), \
      static_cast<PropertyAttribute>(ReadOnly|DontDelete))
  SIZEOF(vorbis_info);
  SIZEOF(vorbis_comment);
  SIZEOF(vorbis_dsp_state);
  SIZEOF(vorbis_block);
  SIZEOF(ogg_packet);

  /* contants */
#define CONST(value) \
  Nan::ForceSet(target, Nan::New<String>(#value).ToLocalChecked(), Nan::New<Integer>(value), \
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
  Nan::SetMethod(target, "vorbis_info_init", node_vorbis_info_init);
  Nan::SetMethod(target, "vorbis_comment_init", node_vorbis_comment_init);
  Nan::SetMethod(target, "vorbis_synthesis_init", node_vorbis_synthesis_init);
  Nan::SetMethod(target, "vorbis_analysis_init", node_vorbis_analysis_init);
  Nan::SetMethod(target, "vorbis_block_init", node_vorbis_block_init);
  Nan::SetMethod(target, "vorbis_encode_init_vbr", node_vorbis_encode_init_vbr);
  Nan::SetMethod(target, "vorbis_analysis_headerout", node_vorbis_analysis_headerout);
  Nan::SetMethod(target, "vorbis_analysis_write", node_vorbis_analysis_write);
  Nan::SetMethod(target, "vorbis_analysis_blockout", node_vorbis_analysis_blockout);
  Nan::SetMethod(target, "vorbis_analysis_eos", node_vorbis_analysis_eos);
  Nan::SetMethod(target, "vorbis_analysis", node_vorbis_analysis);
  Nan::SetMethod(target, "vorbis_bitrate_addblock", node_vorbis_bitrate_addblock);
  Nan::SetMethod(target, "vorbis_bitrate_flushpacket", node_vorbis_bitrate_flushpacket);
  Nan::SetMethod(target, "vorbis_synthesis_idheader", node_vorbis_synthesis_idheader);
  Nan::SetMethod(target, "vorbis_synthesis_headerin", node_vorbis_synthesis_headerin);
  Nan::SetMethod(target, "vorbis_synthesis", node_vorbis_synthesis);
  Nan::SetMethod(target, "vorbis_synthesis_blockin", node_vorbis_synthesis_blockin);
  Nan::SetMethod(target, "vorbis_synthesis_pcmout", node_vorbis_synthesis_pcmout);

  /* custom functions */
  Nan::SetMethod(target, "comment_array", node_comment_array);
  Nan::SetMethod(target, "get_format", node_get_format);

  Nan::ForceSet(target, Nan::New<String>("version").ToLocalChecked(), Nan::New<String>(vorbis_version_string()).ToLocalChecked(),
    static_cast<PropertyAttribute>(ReadOnly|DontDelete));
}

} // nodevorbis namespace

NODE_MODULE(vorbis, nodevorbis::Initialize);
