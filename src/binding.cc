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

/* TODO: async */
Handle<Value> node_vorbis_synthesis_headerin (const Arguments& args) {
  HandleScope scope;
  vorbis_info *vi = UnwrapPointer<vorbis_info *>(args[0]);
  vorbis_comment *vc = UnwrapPointer<vorbis_comment *>(args[1]);
  ogg_packet *op = UnwrapPointer<ogg_packet *>(args[2]);

  int i = vorbis_synthesis_headerin(vi, vc, op);
  return scope.Close(Integer::New(i));
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
  NODE_SET_METHOD(target, "vorbis_synthesis_headerin", node_vorbis_synthesis_headerin);

  target->Set(String::NewSymbol("version"), String::New(vorbis_version_string()),
    static_cast<PropertyAttribute>(ReadOnly|DontDelete));
}

} // nodevorbis namespace

NODE_MODULE(vorbis, nodevorbis::Initialize);
