project(
  'libtextinputflow',
  'c',
  version: '0.0.0',
  license: 'MIT',
  meson_version: '>= 1.3.0',
  default_options: ['c_std=c99', 'warning_level=2'],
)

inc = include_directories('include')

subdir('lib')

if get_option('tests')
  add_languages('cpp', native: false, required: true)
  subdir('tests/lib')
endif
