#include "ogg/ogg.h"
#include "vorbis/codec.h"

namespace nodevorbis {

struct headerin_req {
  uv_work_t req;
  vorbis_info *vi;
  vorbis_comment *vc;
  ogg_packet *op;
  int rtn;
  v8::Persistent<v8::Function> callback;
};

struct synthesis_req {
  uv_work_t req;
  vorbis_block *vb;
  ogg_packet *op;
  int rtn;
  v8::Persistent<v8::Function> callback;
};

struct blockin_req {
  uv_work_t req;
  vorbis_dsp_state *vd;
  vorbis_block *vb;
  int rtn;
  v8::Persistent<v8::Function> callback;
};

struct pcmout_req {
  uv_work_t req;
  vorbis_dsp_state *vd;
  float *buffer;
  int channels;
  int rtn;
  v8::Persistent<v8::Function> callback;
};

/* Decoding */
void node_vorbis_synthesis_headerin_async (uv_work_t *);
void node_vorbis_synthesis_headerin_after (uv_work_t *);

void node_vorbis_synthesis_async (uv_work_t *);
void node_vorbis_synthesis_after (uv_work_t *);

void node_vorbis_synthesis_blockin_async (uv_work_t *);
void node_vorbis_synthesis_blockin_after (uv_work_t *);

void node_vorbis_synthesis_pcmout_async (uv_work_t *);
void node_vorbis_synthesis_pcmout_after (uv_work_t *);

} // nodevorbis namespace
