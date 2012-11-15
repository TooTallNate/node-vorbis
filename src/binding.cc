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

#include <v8.h>
#include <node.h>
#include <string.h>

#include "node_buffer.h"
#include "node_pointer.h"
#include "ogg/ogg.h"
#include "vorbis/codec.h"

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

Handle<Value> node_vorbis_block_init (const Arguments& args) {
  HandleScope scope;
  vorbis_dsp_state *vd = UnwrapPointer<vorbis_dsp_state *>(args[0]);
  vorbis_block *vb = UnwrapPointer<vorbis_block *>(args[1]);
  int r = vorbis_block_init(vd, vb);
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
  format->Set(String::NewSymbol("channels"), Integer::New(vi->channels));
  format->Set(String::NewSymbol("sampleRate"), Number::New(vi->rate));
  format->Set(String::NewSymbol("bitDepth"), Integer::New(sizeof(float) * 8));
  format->Set(String::NewSymbol("float"), True());
  format->Set(String::NewSymbol("signed"), True());
  return scope.Close(format);
}

/* TODO: async */
Handle<Value> node_vorbis_synthesis_headerin (const Arguments& args) {
  HandleScope scope;
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(args[0]);
  vorbis_comment *vc = UnwrapPointer<vorbis_comment *>(args[1]);
  ogg_packet *op = UnwrapPointer<ogg_packet *>(args[2]);

  int i = vorbis_synthesis_headerin(vi, vc, op);
  return scope.Close(Integer::New(i));
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
    // we need to interlace the pcm float data...
    Buffer *buffer = Buffer::New(samples * channels * sizeof(float));
    float *buf = reinterpret_cast<float *>(Buffer::Data(buffer));
    int i, j;
    for (i = 0; i < channels; i++) {
      float *ptr = buf + i;
      float *mono = pcm[i];
      for (j = 0; j < samples; j++) {
        *ptr = mono[j];
        //fprintf(stderr, "%d, %d: %f\n", i, j, *ptr);
        ptr += channels;
      }
    }
    vorbis_synthesis_read(vd, samples);
    rtn = buffer->handle_;
  } else {
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
  NODE_SET_METHOD(target, "vorbis_block_init", node_vorbis_block_init);
  NODE_SET_METHOD(target, "vorbis_synthesis_headerin", node_vorbis_synthesis_headerin);
  NODE_SET_METHOD(target, "vorbis_synthesis", node_vorbis_synthesis);
  NODE_SET_METHOD(target, "vorbis_synthesis_blockin", node_vorbis_synthesis_blockin);
  NODE_SET_METHOD(target, "vorbis_synthesis_pcmout", node_vorbis_synthesis_pcmout);
  NODE_SET_METHOD(target, "comment_array", node_comment_array);
  NODE_SET_METHOD(target, "get_format", node_get_format);

  target->Set(String::NewSymbol("version"), String::New(vorbis_version_string()),
    static_cast<PropertyAttribute>(ReadOnly|DontDelete));
}

} // nodevorbis namespace

NODE_MODULE(vorbis, nodevorbis::Initialize);
