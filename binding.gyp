{
  'targets': [
    {
      'target_name': 'vorbis',
      'include_dirs': [ "<!(node -e \"require('nan')\")" ],
      'sources': [
        'src/binding.cc',
      ],
      'dependencies': [
        'deps/libvorbis/libvorbis.gyp:libvorbis',
        'deps/libvorbis/libvorbis.gyp:vorbisenc',
      ],
    }
  ]
}
