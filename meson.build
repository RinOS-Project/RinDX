project('RinDX', 'c', version: '1.0.0', default_options: ['warning_level=3'])

ringpu_dep = dependency('ringpu', required: true)
rindx_inc = include_directories('include')
rindx = library('rindx', files(
  'src/dxgi/catalog.c',
  'src/dxgi/com.c',
  'src/dxgi/swapchain.c',
  'src/d3d11/d3d11.c',
  'src/d3d12/d3d12.c',
  host_machine.system() == 'windows' ? 'src/d3d11/native_d3d11.c' : [],
  host_machine.system() == 'windows' ? 'src/d3d12/native_d3d12.c' : [],
), include_directories: rindx_inc, dependencies: ringpu_dep, install: true)
rindx_dep = declare_dependency(
  link_with: rindx,
  include_directories: rindx_inc,
  dependencies: ringpu_dep,
)
