project('qrscan', 'c',
  version : '1.0.0',
  license : 'GPL-3.0-or-later',
  default_options : [
    'c_std=c99',
    'warning_level=2',
    'werror=false',
  ]
)

cc = meson.get_compiler('c')

gtk4_dep       = dependency('gtk4',              version : '>= 4.6')
gstreamer_dep  = dependency('gstreamer-1.0',     version : '>= 1.18')
gst_video_dep  = dependency('gstreamer-video-1.0')
gst_app_dep    = dependency('gstreamer-app-1.0')
zbar_dep       = dependency('zbar',              version : '>= 0.23',
                             required : false)
gio_dep        = dependency('gio-2.0')
glib_dep       = dependency('glib-2.0',          version : '>= 2.68')

if not zbar_dep.found()
  zbar_dep = cc.find_library('zbar', required : true)
endif

conf_data = configuration_data()
conf_data.set_quoted('PACKAGE_VERSION', meson.project_version())
conf_data.set_quoted('PACKAGE_NAME',    'QRScan')
conf_data.set_quoted('APP_ID',          'com.hexadecinull.QRScan')
configure_file(output : 'config.h', configuration : conf_data)

src = files(
  'src/main.c',
  'src/app.c',
  'src/scanner_window.c',
  'src/camera_pipeline.c',
  'src/decoder.c',
  'src/result_dialog.c',
  'src/history_store.c',
  'src/creator.c',
  'src/settings.c',
  'src/util.c',
)

executable('qrscan',
  sources      : src,
  dependencies : [gtk4_dep, gstreamer_dep, gst_video_dep, gst_app_dep,
                  zbar_dep, gio_dep, glib_dep],
  include_directories : include_directories('.'),
  install      : true,
  install_dir  : get_option('bindir'),
)

install_data('data/com.hexadecinull.QRScan.desktop',
  install_dir : join_paths(get_option('datadir'), 'applications'))

install_data('data/com.hexadecinull.QRScan.svg',
  install_dir : join_paths(get_option('datadir'), 'icons',
                            'hicolor', 'scalable', 'apps'))
