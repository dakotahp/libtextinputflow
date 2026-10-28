qt6 = import('qt6')
qt6_dep = dependency(
  'qt6',
  modules: ['Core', 'Gui'],
  private_headers: true,
  required: get_option('qt'),
)
if not qt6_dep.found()
  subdir_done()
endif

qt_plugin_dir = meson.current_build_dir()
qt_plugin_sources = files('textinputflowtheme.cpp')
qt_plugin_moc = qt6.compile_moc(sources: qt_plugin_sources, dependencies: qt6_dep)
qt_plugin_inc = include_directories('.')

subdir('platformthemes')

if get_option('tests')
  subdir('tests')
endif
