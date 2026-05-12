project('Hyprspace', 'cpp',
  version: '0.1',
  default_options: [
    'buildtype=release',
    'cpp_std=c++23' 
  ],
)

add_project_arguments([ '-DWLR_USE_UNSTABLE' ], language: 'cpp')

src = files(
  'src/main.cpp',
  'src/Render.cpp',
  'src/Input.cpp',
  'src/Layout.cpp',
  'src/Overview.cpp'
)

shared_module(meson.project_name(), src,
  dependencies: [
    dependency('hyprland'),
    dependency('hyprutils'),   # Provides CHyprColor, Vector2D structures
    dependency('hyprlang'),    # Provides CConfigValue definitions
    dependency('pixman-1'),
    dependency('libdrm'),
    dependency('wayland-server'),
    dependency('xkbcommon'),
  ],
  install: true,
)
