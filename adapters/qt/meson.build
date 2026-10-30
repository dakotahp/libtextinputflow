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

qt_plugin_install_dir = get_option('qt_plugin_dir')
if qt_plugin_install_dir == ''
  qmake6 = find_program('qmake6', 'qmake-qt6')
  qt_plugin_install_dir = run_command(qmake6, '-query', 'QT_INSTALL_PLUGINS', check: true).stdout().strip()
endif

subdir('platformthemes')

if get_option('tests')
  subdir('tests')
endif
