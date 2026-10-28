test_c_args = ['-D_XOPEN_SOURCE=700']

foreach name : ['test_null', 'test_header_c99', 'test_parse', 'test_merge']
  test(
    name,
    executable(name, name + '.c', c_args: test_c_args, dependencies: textinputflow_dep),
  )
endforeach

foreach name : ['test_default_file', 'test_stub_fixture']
  test(
    name,
    executable(name, name + '.c', c_args: test_c_args, dependencies: textinputflow_dep),
    workdir: meson.project_source_root(),
  )
endforeach

test(
  'test_paths',
  executable(
    'test_paths',
    'test_paths.c',
    '../../lib/paths.c',
    c_args: test_c_args,
    include_directories: include_directories('../../lib'),
    dependencies: textinputflow_dep,
  ),
)

if have_cpp
  test(
    'test_header_cpp',
    executable(
      'test_header_cpp',
      'test_header_cpp.cpp',
      dependencies: textinputflow_dep,
      override_options: ['cpp_std=c++17'],
    ),
  )
endif
