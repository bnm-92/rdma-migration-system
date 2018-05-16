project('rdma-migration-system', 'cpp')
cxx = meson.get_compiler('cpp')


deps = [
    cxx.find_library('rdmacm'),
    cxx.find_library('ibverbs'),
    cxx.find_library('pthread'),
]

inc = include_directories('include')

base_dir = 'src/rdma-network/'
sources = [base_dir + 'util.cpp', base_dir + 'rdma_server_prototype.cpp',
    base_dir + 'rdma_client.cpp', base_dir + 'rdma_server.cpp']

shared_library('rdma',
    sources,
    include_directories : inc,
    dependencies: deps,
    install : true)
