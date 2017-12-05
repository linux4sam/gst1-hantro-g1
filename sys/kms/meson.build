g1kmssink_sources = [
  'gstkmsallocator.c',
  'gstkmsbufferpool.c',
  'gstg1kmssink.c',
  'gstkmsutils.c',
]

libdrm_dep = dependency('libdrm', version : '>= 2.4.55', required : false)

if libdrm_dep.found()
  gstg1kmssink = library('gstg1kms',
    g1kmssink_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstbase_dep, gstvideo_dep, gstallocators_dep, libdrm_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
endif
