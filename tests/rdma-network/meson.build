project('rdma-migration-system-test', 'cpp')

cxx = meson.get_compiler('cpp')

lib_dep = declare_dependency(
  dependencies : cxx.find_library('rdma', dirs : '/home/sdh/Downloads/rdma-migration-system/build'),
  include_directories : include_directories('/home/sdh/Downloads/rdma-migration-system/include'))

executable('prog', 'test-server.cpp', dependencies : lib_dep)
