{
  'targets': [
    {
      'target_name': 'vorbis',
      'sources': [
        'src/binding.cc',
      ],
      'dependencies': [
        'deps/libvorbis/libvorbis.gyp:vorbis',
      ],
    }
  ]
}
