# The libvorbis.gyp file uses the "ogg_include_dirs" and "ogg_libraries"
# variables to specify custom locations of the libogg headers and symbols.
# For 'node-vorbis', we want to find the 'node-ogg' module so that we can link
# directly to that .node module.
{
  'variables': {
    'node_ogg': '<!(node -p -e "require(\'path\').dirname(require.resolve(\'ogg\'))")',
    'ogg_include_dirs': [
      '<(node_ogg)/deps/libogg/include',
      '<(node_ogg)/deps/libogg/config/<(OS)/<(target_arch)',
    ],
    'conditions': [
      ['OS=="win"', {
        'ogg_libraries': [
          '<(node_ogg)/build/$(Configuration)/ogg.lib',
        ],
      }, {
        'ogg_libraries': [
          '<(node_ogg)/build/$(BUILDTYPE)/ogg.node',
          '-Wl,-rpath,<(node_ogg)/build/$(BUILDTYPE)',
        ],
      }],
    ],
  },
}
