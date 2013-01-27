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
#include <string.h>

#include "binding.h"
#include "node_buffer.h"
#include "node_pointer.h"
#include "ogg/ogg.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisenc.h"

using namespace v8;
using namespace node;

namespace nodevorbis {


Handle<Value> node_vorbis_info_init (const Arguments& args) {
  HandleScope scope;
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(args[0]);
  vorbis_info_init(vi);
  return Undefined();
}


Handle<Value> node_vorbis_comment_init (const Arguments& args) {
  HandleScope scope;
  vorbis_comment *vc = UnwrapPointer<vorbis_comment *>(args[0]);
  vorbis_comment_init(vc);
  return Undefined();
}


Handle<Value> node_vorbis_synthesis_init (const Arguments& args) {
  HandleScope scope;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(args[1]);
  int r = vorbis_synthesis_init(vd, vi);
  return scope.Close(Integer::New(r));
}


Handle<Value> node_vorbis_analysis_init (const Arguments& args) {
  HandleScope scope;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(args[1]);
  int r = vorbis_analysis_init(vd, vi);
  return scope.Close(Integer::New(r));
}


Handle<Value> node_vorbis_block_init (const Arguments& args) {
  HandleScope scope;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(args[1]);
  int r = vorbis_block_init(vd, vb);
  return scope.Close(Integer::New(r));
}


Handle<Value> node_vorbis_encode_init_vbr (const Arguments& args) {
  HandleScope scope;
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(args[0]);
  long channels = args[1]->IntegerValue();
  long rate = args[2]->IntegerValue();
  float quality = args[3]->NumberValue();
  int r = vorbis_encode_init_vbr(vi, channels, rate, quality);
  return scope.Close(Integer::New(r));
}


Handle<Value> node_comment_array (const Arguments& args) {
  HandleScope scope;
  int i;
  vorbis_comment *vc = UnwrapPointer<vorbis_comment *>(args[0]);
  Local<Array> array = Array::New(vc->comments);
  for (i = 0; i < vc->comments; i++) {
    array->Set(i, String::New(vc->user_comments[i], vc->comment_lengths[i]));
  }
  array->Set(String::NewSymbol("vendor"), String::New(vc->vendor));
  return scope.Close(array);
}


Handle<Value> node_get_format (const Arguments& args) {
  HandleScope scope;
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(args[0]);
  Local<Object> format = Object::New();

  /* PCM format properties */
  format->Set(String::NewSymbol("channels"), Integer::New(vi->channels));
  format->Set(String::NewSymbol("sampleRate"), Number::New(vi->rate));
  format->Set(String::NewSymbol("bitDepth"), Integer::New(sizeof(float) * 8));
  format->Set(String::NewSymbol("float"), True());
  format->Set(String::NewSymbol("signed"), True());

  /* Other random info from the vorbis_info struct */
  format->Set(String::NewSymbol("version"), Integer::New(vi->version));
  format->Set(String::NewSymbol("bitrateUpper"), Number::New(vi->bitrate_upper));
  format->Set(String::NewSymbol("bitrateNominal"), Number::New(vi->bitrate_nominal));
  format->Set(String::NewSymbol("bitrateLower"), Number::New(vi->bitrate_lower));
  format->Set(String::NewSymbol("bitrateWindow"), Number::New(vi->bitrate_window));

  return scope.Close(format);
}


/* TODO: async */
Handle<Value> node_vorbis_analysis_headerout (const Arguments& args) {
  HandleScope scope;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  vorbis_comment *vc = UnwrapPointer<vorbis_comment *>(args[1]);
  ogg_packet *op_header = UnwrapPointer<ogg_packet *>(args[2]);
  ogg_packet *op_comments = UnwrapPointer<ogg_packet *>(args[3]);
  ogg_packet *op_code = UnwrapPointer<ogg_packet *>(args[4]);
  int r = vorbis_analysis_headerout(vd, vc, op_header, op_comments, op_code);
  return scope.Close(Integer::New(r));
}


/* combination of `vorbis_analysis_buffer()`, `memcpy()`, and
 * `vorbis_analysis_wrote()` on the thread pool. */
Handle<Value> node_vorbis_analysis_write (const Arguments& args) {
  HandleScope scope;
  Local<Function> callback = Local<Function>::Cast(args[4]);

  write_req *r = new write_req;
  r->vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  r->buffer = UnwrapPointer<float *>(args[1]);
  r->channels = args[2]->IntegerValue();
  r->samples = args[3]->NumberValue();
  r->callback = Persistent<Function>::New(callback);
  r->rtn = 0;
  r->req.data = r;

  uv_queue_work(uv_default_loop(),
                &r->req,
                node_vorbis_analysis_write_async,
                (uv_after_work_cb)node_vorbis_analysis_write_after);
  return Undefined();
}

void node_vorbis_analysis_write_async (uv_work_t *req) {
  write_req *r = reinterpret_cast<write_req *>(req->data);

  /* interleaved float samples */
  float *input = r->buffer;
  long samples = r->samples;
  int channels = r->channels;
  int i = 0, j = 0;

  /* expose buffer to write PCM float samples to */
  float **output = vorbis_analysis_buffer(r->vd, samples);

  /* uninterleave samples */
  for (i = 0; i < samples; i++) {
    for (j = 0; j < channels; j++) {
      output[j][i] = input[samples * channels + j];
    }
  }

  /* tell the library how much we actually submitted */
  r->rtn = vorbis_analysis_wrote(r->vd, i);
}

void node_vorbis_analysis_write_after (uv_work_t *req) {
  HandleScope scope;
  write_req *r = reinterpret_cast<write_req *>(req->data);

  Handle<Value> argv[1] = { Integer::New(r->rtn) };

  TryCatch try_catch;
  r->callback->Call(Context::GetCurrent()->Global(), 1, argv);

  // cleanup
  r->callback.Dispose();
  delete r;

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


/* vorbis_analysis_blockout() on the thread pool */
Handle<Value> node_vorbis_analysis_blockout (const Arguments& args) {
  HandleScope scope;
  Local<Function> callback = Local<Function>::Cast(args[2]);

  blockout_req *r = new blockout_req;
  r->vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  r->vb = UnwrapPointer<vorbis_block *>(args[1]);
  r->callback = Persistent<Function>::New(callback);
  r->rtn = 0;
  r->req.data = r;

  uv_queue_work(uv_default_loop(),
                &r->req,
                node_vorbis_analysis_blockout_async,
                (uv_after_work_cb)node_vorbis_analysis_blockout_after);
  return Undefined();
}

void node_vorbis_analysis_blockout_async (uv_work_t *req) {
  blockout_req *r = reinterpret_cast<blockout_req *>(req->data);
  r->rtn = vorbis_analysis_blockout(r->vd, r->vb);
}

void node_vorbis_analysis_blockout_after (uv_work_t *req) {
  HandleScope scope;
  blockout_req *r = reinterpret_cast<blockout_req *>(req->data);

  Handle<Value> argv[1] = { Integer::New(r->rtn) };

  TryCatch try_catch;
  r->callback->Call(Context::GetCurrent()->Global(), 1, argv);

  // cleanup
  r->callback.Dispose();
  delete r;

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}

/* TODO: async? */
Handle<Value> node_vorbis_analysis_eos (const Arguments& args) {
  HandleScope scope;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);

  int i = vorbis_analysis_wrote(vd, 0);
  return scope.Close(Integer::New(i));
}

/* TODO: async? */
Handle<Value> node_vorbis_analysis (const Arguments& args) {
  HandleScope scope;
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(args[0]);
  ogg_packet *op = UnwrapPointer<ogg_packet *>(args[1]);

  int i = vorbis_analysis(vb, op);
  return scope.Close(Integer::New(i));
}

/* TODO: async? */
Handle<Value> node_vorbis_bitrate_addblock (const Arguments& args) {
  HandleScope scope;
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(args[0]);

  int i = vorbis_bitrate_addblock(vb);
  return scope.Close(Integer::New(i));
}


/* vorbis_bitrate_flushpacket() on the thread pool */
Handle<Value> node_vorbis_bitrate_flushpacket (const Arguments& args) {
  HandleScope scope;
  Local<Function> callback = Local<Function>::Cast(args[2]);

  flushpacket_req *r = new flushpacket_req;
  r->vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  r->op = UnwrapPointer<ogg_packet *>(args[1]);
  r->callback = Persistent<Function>::New(callback);
  r->rtn = 0;
  r->req.data = r;

  uv_queue_work(uv_default_loop(),
                &r->req,
                node_vorbis_bitrate_flushpacket_async,
                (uv_after_work_cb)node_vorbis_bitrate_flushpacket_after);
  return Undefined();
}

void node_vorbis_bitrate_flushpacket_async (uv_work_t *req) {
  flushpacket_req *r = reinterpret_cast<flushpacket_req *>(req->data);
  r->rtn = vorbis_bitrate_flushpacket(r->vd, r->op);
}

void node_vorbis_bitrate_flushpacket_after (uv_work_t *req) {
  HandleScope scope;
  flushpacket_req *r = reinterpret_cast<flushpacket_req *>(req->data);

  Handle<Value> argv[1] = { Integer::New(r->rtn) };

  TryCatch try_catch;
  r->callback->Call(Context::GetCurrent()->Global(), 1, argv);

  // cleanup
  r->callback.Dispose();
  delete r;

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


/* vorbis_synthesis_idheader() called on the thread pool */
Handle<Value> node_vorbis_synthesis_idheader (const Arguments& args) {
  HandleScope scope;
  Local<Function> callback = Local<Function>::Cast(args[1]);

  idheader_req *r = new idheader_req;
  r->op = UnwrapPointer<ogg_packet *>(args[0]);
  r->callback = Persistent<Function>::New(callback);
  r->rtn = 0;
  r->req.data = r;

  uv_queue_work(uv_default_loop(),
                &r->req,
                node_vorbis_synthesis_idheader_async,
                (uv_after_work_cb)node_vorbis_synthesis_idheader_after);
  return Undefined();
}

void node_vorbis_synthesis_idheader_async (uv_work_t *req) {
  idheader_req *r = reinterpret_cast<idheader_req *>(req->data);
  r->rtn = vorbis_synthesis_idheader(r->op);
}

void node_vorbis_synthesis_idheader_after (uv_work_t *req) {
  HandleScope scope;
  idheader_req *r = reinterpret_cast<idheader_req *>(req->data);

  Handle<Value> argv[] = { Null(), Boolean::New(r->rtn) };

  TryCatch try_catch;
  r->callback->Call(Context::GetCurrent()->Global(), 2, argv);

  // cleanup
  r->callback.Dispose();
  delete r;

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


/* vorbis_synthesis_headerin() called on the thread pool */
Handle<Value> node_vorbis_synthesis_headerin (const Arguments& args) {
  HandleScope scope;
  Local<Function> callback = Local<Function>::Cast(args[3]);

  headerin_req *r = new headerin_req;
  r->vi = UnwrapPointer<vorbis_info *>(args[0]);
  r->vc = UnwrapPointer<vorbis_comment *>(args[1]);
  r->op = UnwrapPointer<ogg_packet *>(args[2]);
  r->rtn = 0;
  r->callback = Persistent<Function>::New(callback);
  r->req.data = r;

  uv_queue_work(uv_default_loop(),
                &r->req,
                node_vorbis_synthesis_headerin_async,
                (uv_after_work_cb)node_vorbis_synthesis_headerin_after);
  return Undefined();
}

void node_vorbis_synthesis_headerin_async (uv_work_t *req) {
  headerin_req *r = reinterpret_cast<headerin_req *>(req->data);
  r->rtn = vorbis_synthesis_headerin(r->vi, r->vc, r->op);
}

void node_vorbis_synthesis_headerin_after (uv_work_t *req) {
  HandleScope scope;
  headerin_req *r = reinterpret_cast<headerin_req *>(req->data);

  Handle<Value> argv[1] = { Integer::New(r->rtn) };

  TryCatch try_catch;
  r->callback->Call(Context::GetCurrent()->Global(), 1, argv);

  // cleanup
  r->callback.Dispose();
  delete r;

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


/* TODO: async */
Handle<Value> node_vorbis_synthesis (const Arguments& args) {
  HandleScope scope;
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(args[0]);
  ogg_packet *op = UnwrapPointer<ogg_packet *>(args[1]);

  int i = vorbis_synthesis(vb, op);
  return scope.Close(Integer::New(i));
}


/* TODO: async */
Handle<Value> node_vorbis_synthesis_blockin (const Arguments& args) {
  HandleScope scope;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(args[1]);

  int i = vorbis_synthesis_blockin(vd, vb);
  return scope.Close(Integer::New(i));
}


/* TODO: async */
Handle<Value> node_vorbis_synthesis_pcmout (const Arguments& args) {
  HandleScope scope;
  float **pcm;
  int samples;
  Handle<Value> rtn;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  int channels = args[1]->Int32Value();

  samples = vorbis_synthesis_pcmout(vd, &pcm);

  if (samples > 0) {
    /* we need to interlace the pcm float data... */
    Buffer *buffer = Buffer::New(samples * channels * sizeof(float));
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
    rtn = buffer->handle_;
  } else {
    /* an error or 0 samples */
    rtn = Integer::New(samples);
  }

  return scope.Close(rtn);
}


void Initialize(Handle<Object> target) {
  HandleScope scope;

  /* sizeof's */
#define SIZEOF(value) \
  target->Set(String::NewSymbol("sizeof_" #value), Integer::New(sizeof(value)), \
      static_cast<PropertyAttribute>(ReadOnly|DontDelete))
  SIZEOF(vorbis_info);
  SIZEOF(vorbis_comment);
  SIZEOF(vorbis_dsp_state);
  SIZEOF(vorbis_block);
  SIZEOF(ogg_packet);

  /* contants */
#define CONST(value) \
  target->Set(String::NewSymbol(#value), Integer::New(value), \
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

  target->Set(String::NewSymbol("version"), String::New(vorbis_version_string()),
    static_cast<PropertyAttribute>(ReadOnly|DontDelete));
}

} // nodevorbis namespace

NODE_MODULE(vorbis, nodevorbis::Initialize);
