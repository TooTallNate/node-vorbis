#include "ogg/ogg.h"
#include "vorbis/codec.h"

namespace nodevorbis {

struct write_req {
  uv_work_t req;
  vorbis_dsp_state *vd;
  float *buffer;
  int channels;
  long samples;
  int rtn;
  NanPersistent<v8::Function> callback;
};

struct flushpacket_req {
  uv_work_t req;
  vorbis_dsp_state *vd;
  ogg_packet *op;
  int rtn;
  NanPersistent<v8::Function> callback;
};

struct idheader_req {
  uv_work_t req;
  ogg_packet *op;
  int rtn;
  NanPersistent<v8::Function> callback;
};

struct headerin_req {
  uv_work_t req;
  vorbis_info *vi;
  vorbis_comment *vc;
  ogg_packet *op;
  int rtn;
  NanPersistent<v8::Function> callback;
};

struct synthesis_req {
  uv_work_t req;
  vorbis_block *vb;
  ogg_packet *op;
  int rtn;
  NanPersistent<v8::Function> callback;
};

struct blockin_req {
  uv_work_t req;
  vorbis_dsp_state *vd;
  vorbis_block *vb;
  int rtn;
  NanPersistent<v8::Function> callback;
};

typedef blockin_req blockout_req;

struct pcmout_req {
  uv_work_t req;
  vorbis_dsp_state *vd;
  float *buffer;
  int channels;
  int rtn;
  NanPersistent<v8::Function> callback;
};

/* Encoding */
void node_vorbis_analysis_write_async (uv_work_t *);
void node_vorbis_analysis_write_after (uv_work_t *);

void node_vorbis_analysis_blockout_async (uv_work_t *);
void node_vorbis_analysis_blockout_after (uv_work_t *);

void node_vorbis_bitrate_flushpacket_async (uv_work_t *);
void node_vorbis_bitrate_flushpacket_after (uv_work_t *);

/* Decoding */
void node_vorbis_synthesis_idheader_async (uv_work_t *);
void node_vorbis_synthesis_idheader_after (uv_work_t *);

void node_vorbis_synthesis_headerin_async (uv_work_t *);
void node_vorbis_synthesis_headerin_after (uv_work_t *);

void node_vorbis_synthesis_async (uv_work_t *);
void node_vorbis_synthesis_after (uv_work_t *);

void node_vorbis_synthesis_blockin_async (uv_work_t *);
void node_vorbis_synthesis_blockin_after (uv_work_t *);

void node_vorbis_synthesis_pcmout_async (uv_work_t *);
void node_vorbis_synthesis_pcmout_after (uv_work_t *);

} // nodevorbis namespace
