{
  'targets': [
    {
      'target_name': 'vorbis',
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
