stub_dir = meson.project_source_root() / 'tests' / 'fixtures' / 'stub'

test(
  'qt_keybindings',
  executable(
    'test_qt_keybindings',
    'test_keybindings.cpp',
    dependencies: qt6_dep,
    override_options: ['cpp_std=c++17'],
  ),
  depends: textinputflow_qt,
  env: {
    'QT_QPA_PLATFORM': 'offscreen',
    'QT_QPA_PLATFORMTHEME': 'textinputflow',
    'QT_PLUGIN_PATH': qt_plugin_dir,
    'TEXTINPUTFLOW_QT_BASE_THEME': 'nonexistent',
    'XDG_CONFIG_HOME': stub_dir,
    'XDG_DATA_DIRS': stub_dir,
  },
)

qt6_widgets_dep = dependency(
  'qt6',
  modules: ['Core', 'Gui', 'Widgets'],
  private_headers: true,
  required: false,
)
if qt6_widgets_dep.found()
  test(
    'qt_shortcut_filter',
    executable(
      'test_qt_shortcut_filter',
      'test_shortcut_filter.cpp',
      dependencies: qt6_widgets_dep,
      override_options: ['cpp_std=c++17'],
    ),
    depends: textinputflow_qt,
    env: {
      'QT_QPA_PLATFORM': 'offscreen',
      'QT_QPA_PLATFORMTHEME': 'textinputflow',
      'QT_PLUGIN_PATH': qt_plugin_dir,
      'TEXTINPUTFLOW_QT_BASE_THEME': 'nonexistent',
      'XDG_CONFIG_HOME': stub_dir,
      'XDG_DATA_DIRS': stub_dir + ':/usr/local/share:/usr/share',
    },
  )

  executable(
    'themedump',
    'themedump.cpp',
    dependencies: qt6_widgets_dep,
    override_options: ['cpp_std=c++17'],
  )
endif
