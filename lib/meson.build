libtextinputflow = library(
  'textinputflow',
  'textinputflow.c',
  'parse.c',
  'paths.c',
  'table.c',
  include_directories: inc,
  gnu_symbol_visibility: 'hidden',
  version: meson.project_version(),
  soversion: '0',
  install: true,
)

install_headers('../include/textinputflow.h')

textinputflow_dep = declare_dependency(
  link_with: libtextinputflow,
  include_directories: inc,
)
meson.override_dependency('textinputflow', textinputflow_dep)

pkg = import('pkgconfig')
pkg.generate(
  libtextinputflow,
  name: 'textinputflow',
  description: 'Shared text-editing key bindings for Linux toolkits',
)
