project('tpre',
  'c', 'cpp',
  version: '1.0.0-pre.1',
  default_options: ['c_std=c99'])

libtprec = static_library('tprec',
  'compiler/utils.c',
  'compiler/lexer.c',
  'compiler/parser.c',
  'compiler/compiler.c',
  include_directories: './include',
  install: true)

libtprert = static_library('tprert',
  'runtime.c',
  include_directories: './include',
  install: true)

install_headers(
  'include/tpre.h',
  'include/tpre_common.h',
  'include/tpre_compiler.h',
  'include/tpre_runtime.h')

dep_tprec = declare_dependency(
  link_with: libtprec,
  include_directories: './include')

dep_tprert = declare_dependency(
  link_with: libtprert,
  include_directories: './include')

test('test-cartrain', executable('test-cartrain',
  './tests/cartrain.c',
  dependencies: [dep_tprert,dep_tprec]))

test('test-char-classes', executable('test-char-classes',
  './tests/char_classes.c',
  dependencies: [dep_tprert,dep_tprec]))

test('test-repeats', executable('test-repeats',
  './tests/repeats.c',
  dependencies: [dep_tprert,dep_tprec]))

test('test-regression', executable('test-regression',
  './tests/regression.c',
  dependencies: [dep_tprert,dep_tprec]))

test('example', executable('example',
  'example.c',
  dependencies: [dep_tprert,dep_tprec]))
