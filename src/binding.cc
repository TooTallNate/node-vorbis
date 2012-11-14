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
  target->Set(String::NewSymbol("sizeof_vorbis_info"), Integer::New(sizeof(vorbis_info)));
  target->Set(String::NewSymbol("sizeof_vorbis_comment"), Integer::New(sizeof(vorbis_comment)));

  NODE_SET_METHOD(target, "vorbis_synthesis_headerin", node_vorbis_synthesis_headerin);
}

} // nodevorbis namespace

NODE_MODULE(vorbis, nodevorbis::Initialize);
