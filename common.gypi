# 
{
  'variables': {
    'node_ogg': '<!(node find-ogg.js)',
    'ogg_include_dirs': [
      '<(node_ogg)/deps/libogg/include',
    ],
    'ogg_libraries': [
      '<(node_ogg)/build/Release/ogg.node',
      '-Wl,-rpath,<(node_ogg)/build/Release',
    ],
  },
}
