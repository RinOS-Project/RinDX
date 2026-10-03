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

rindx_d3d11_runtime_test = executable(
  'rindx-d3d11-runtime-test',
  'tests/rindx_d3d11_runtime_test.c',
  dependencies: rindx_dep,
)
test('rindx-d3d11-runtime', rindx_d3d11_runtime_test)

rindx_d3d12_runtime_test = executable(
  'rindx-d3d12-runtime-test',
  'tests/rindx_d3d12_runtime_test.c',
  dependencies: rindx_dep,
)
test('rindx-d3d12-runtime', rindx_d3d12_runtime_test)

if host_machine.system() == 'windows'
  rindx_native_d3d_runtime_test = executable(
    'rindx-native-d3d-runtime-test',
    'tests/rindx_native_d3d_runtime_test.c',
    dependencies: rindx_dep,
  )
  test('rindx-native-d3d-runtime', rindx_native_d3d_runtime_test)
endif
