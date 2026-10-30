project(
  'libtextinputflow',
  'c',
  version: '0.1.0',
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

qt_option = get_option('qt')
if not qt_option.disabled() and add_languages('cpp', native: false, required: qt_option)
  subdir('adapters/qt')
endif
