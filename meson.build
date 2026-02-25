project('tpre',
  'c', 'cpp',
  version: '1.0.0-pre.1',
  default_options: ['c_std=c99'])

libtpre = static_library('tpre',
  'tpre.c',
  install: true)

install_headers('include/tpre.h')

dep_tpre = declare_dependency(
  link_with: libtpre,
  include_directories: './include')

test('test-tpre', executable('test-tpre',
  'test.c',
  dependencies: dep_tpre))

test('example', executable('example',
  'example.c',
  dependencies: dep_tpre))
