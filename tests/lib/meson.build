foreach name : ['test_stub', 'test_null', 'test_header_c99']
  test(name, executable(name, name + '.c', dependencies: textinputflow_dep))
endforeach

test(
  'test_header_cpp',
  executable(
    'test_header_cpp',
    'test_header_cpp.cpp',
    dependencies: textinputflow_dep,
    override_options: ['cpp_std=c++17'],
  ),
)
