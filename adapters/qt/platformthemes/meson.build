textinputflow_qt = shared_module(
  'textinputflow-qt',
  qt_plugin_sources,
  qt_plugin_moc,
  include_directories: qt_plugin_inc,
  dependencies: [qt6_dep, textinputflow_dep],
  gnu_symbol_visibility: 'hidden',
  override_options: ['cpp_std=c++17'],
)
