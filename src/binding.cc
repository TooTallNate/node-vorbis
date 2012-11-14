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

Handle<Value> node_vorbis_synthesis_headerin (const Arguments& args) {
  HandleScope scope;
  int i = vorbis_synthesis_headerin(NULL, NULL, NULL);
  oggpack_writeinit(NULL);
  return scope.Close(Undefined());
}

void Initialize(Handle<Object> target) {
  HandleScope scope;

  NODE_SET_METHOD(target, "vorbis_systhesis_headerin", node_vorbis_synthesis_headerin);
  /*
  target->Set(String::NewSymbol("sizeof_ogg_sync_state"), Integer::New(sizeof(ogg_sync_state)));
  target->Set(String::NewSymbol("sizeof_ogg_stream_state"), Integer::New(sizeof(ogg_stream_state)));
  target->Set(String::NewSymbol("sizeof_ogg_page"), Integer::New(sizeof(ogg_page)));
  target->Set(String::NewSymbol("sizeof_ogg_packet"), Integer::New(sizeof(ogg_packet)));

  NODE_SET_METHOD(target, "ogg_sync_write", node_ogg_sync_write);
  NODE_SET_METHOD(target, "ogg_sync_pageout", node_ogg_sync_pageout);

  NODE_SET_METHOD(target, "ogg_stream_init", node_ogg_stream_init);
  NODE_SET_METHOD(target, "ogg_stream_pagein", node_ogg_stream_pagein);
  NODE_SET_METHOD(target, "ogg_stream_packetout", node_ogg_stream_packetout);

  NODE_SET_METHOD(target, "ogg_packet_packetno", node_ogg_packet_packetno);
  NODE_SET_METHOD(target, "ogg_packet_bytes", node_ogg_packet_bytes);
  */
}

} // nodevorbis namespace

NODE_MODULE(vorbis, nodevorbis::Initialize);
