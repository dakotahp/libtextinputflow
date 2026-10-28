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
subdir('data')

if get_option('tests')
  have_cpp = add_languages('cpp', native: false, required: false)
  subdir('tests/lib')
endif
