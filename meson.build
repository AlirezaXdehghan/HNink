project('hnink', 'c', version: '0.99.0-beta.1', default_options: ['c_std=c11', 'warning_level=2'])

add_project_arguments('-DHNINK_VERSION="' + meson.project_version() + '"', language: 'c')

gtk = dependency('gtk+-2.0', version: '>=2.18')
executable('hnink-ui', 'src/main.c', dependencies: [gtk, dependency('cairo'), dependency('atk'), dependency('pango')], install: true)

go = find_program('go', native: true)
if host_machine.system() == 'linux' and host_machine.cpu_family() == 'arm'
  go_os = 'linux'
  go_arch = 'arm'
  go_extra = ['GOARM=7']
elif host_machine.system() == 'darwin' and host_machine.cpu_family() == 'aarch64'
  go_os = 'darwin'
  go_arch = 'arm64'
  go_extra = []
elif host_machine.system() == 'linux' and host_machine.cpu_family() == 'x86_64'
  go_os = 'linux'
  go_arch = 'amd64'
  go_extra = []
elif host_machine.system() == 'linux' and host_machine.cpu_family() == 'aarch64'
  go_os = 'linux'
  go_arch = 'arm64'
  go_extra = []
else
  error('Unsupported build target for Go data service')
endif

# Pinned: Go 1.24+ needs Linux 3.2, but Kindles run 2.6.31 to 3.0.x (PW1 to Oasis 2).
# GOTOOLCHAIN downloads this exact release if the installed Go differs.
go_toolchain = 'go1.23.12'

custom_target('hnink-data',
  input: ['backend/main.go', 'backend/account.go'],
  output: 'hnink-data',
  command: ['env', 'CGO_ENABLED=0', 'GOTOOLCHAIN=' + go_toolchain, 'GOOS=' + go_os, 'GOARCH=' + go_arch] + go_extra +
    [go, 'build', '-trimpath', '-ldflags=-s -w -X main.version=' + meson.project_version(), '-o', '@OUTPUT@', '@INPUT@'],
  build_by_default: true,
  install: true,
  install_dir: get_option('bindir'),
)
