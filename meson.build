project(
    'mkdcdisc', ['c', 'cpp'],
    default_options: ['c_std=c99', 'cpp_std=c++17'],
    version: '0.0.2'
)

#
## Host detection
#

c_compiler = meson.get_compiler('c')
is_windows_msys2 = (run_command('sh', '-c', 'echo "$MSYSTEM"', check: false).stdout().strip()) == 'MSYS'
is_windows_mingw64 = host_machine.system() == 'windows' and not is_windows_msys2
is_windows = is_windows_msys2 or is_windows_mingw64
is_linux = host_machine.system() == 'linux' or is_windows_msys2

#
## Variables
#

project_link_libraries = []
project_link_extra_args = []
project_deps = []
project_include_directories = ['src']

is_install_required = is_linux

project_sources = files(
    'src/main.cpp', 
    'src/scramble.cpp', 
    'src/elf_parser.cpp'
)

# Use system libisofs for all OS except MinGW-w64 (where we will use local libisofs)
if not is_windows_mingw64
	project_deps += [
		dependency('libisofs-1', required: true)
    ]
endif

#
## Windows additions for MinGW-w64 environment (but NOT FOR MSYS2!)
#

# MSYS2 is handled like a regular Unix system, as it will uses the POSIX layer
# emulation; while regular MinGW-w64 is pure native Windows solution, where
# this POSIX layer isn't available. It means for MSYS2, we can use the
# regular libisofs package from source, where for MinGW-w64 we need to use
# our patched copy provided here.

if is_windows_mingw64
    zlib_dep = dependency('zlib', required: true)
    iconv_dep = dependency('iconv', required: true)		
    project_deps += [
		zlib_dep,
        iconv_dep,
    ]

    libisofs_compile_args = [
        '-DHAVE_CONFIG_H=1',        
        '-DWIN32_LEAN_AND_MEAN',
#		'-D_DEBUG=1'
    ]

    libisofs = static_library(
        'isofs',
        [
			'src/third_party/libisofs/libisofs/builder.c',			
			'src/third_party/libisofs/libisofs/node.c',			
			'src/third_party/libisofs/libisofs/tree.c',
			'src/third_party/libisofs/libisofs/find.c',			
			'src/third_party/libisofs/libisofs/image.c',			
			'src/third_party/libisofs/libisofs/fsource.c',
			'src/third_party/libisofs/libisofs/fs_local.c',
			'src/third_party/libisofs/libisofs/fs_image.c',		
			'src/third_party/libisofs/libisofs/messages.c',			
			'src/third_party/libisofs/libisofs/libiso_msgs.c',			
			'src/third_party/libisofs/libisofs/stream.c',			
			'src/third_party/libisofs/libisofs/filter.c',
#			'src/third_party/libisofs/libisofs/filters/external.c',
			'src/third_party/libisofs/libisofs/filters/zisofs.c',
			'src/third_party/libisofs/libisofs/filters/gzip.c',			
			'src/third_party/libisofs/libisofs/util.c',
			'src/third_party/libisofs/libisofs/util_rbtree.c',
			'src/third_party/libisofs/libisofs/util_htable.c',			
			'src/third_party/libisofs/libisofs/filesrc.c',			
			'src/third_party/libisofs/libisofs/ecma119.c',			
			'src/third_party/libisofs/libisofs/ecma119_tree.c',			
			'src/third_party/libisofs/libisofs/buffer.c',
			'src/third_party/libisofs/libisofs/rockridge.c',
			'src/third_party/libisofs/libisofs/rockridge_read.c',
			'src/third_party/libisofs/libisofs/joliet.c',
			'src/third_party/libisofs/libisofs/hfsplus.c',
			'src/third_party/libisofs/libisofs/hfsplus_decompose.c',
			'src/third_party/libisofs/libisofs/hfsplus_classes.c',
			'src/third_party/libisofs/libisofs/hfsplus_case.c',
			'src/third_party/libisofs/libisofs/eltorito.c',
			'src/third_party/libisofs/libisofs/system_area.c',
			'src/third_party/libisofs/libisofs/make_isohybrid_mbr.c',
			'src/third_party/libisofs/libisofs/iso1999.c',
			'src/third_party/libisofs/libisofs/data_source.c',
			'src/third_party/libisofs/libisofs/aaip_0_2.c',
			'src/third_party/libisofs/libisofs/md5.c'
        ],
        include_directories: ['src/third_party/libisofs/libisofs'],
        dependencies: [zlib_dep, iconv_dep],
        c_args: libisofs_compile_args,
        install: false
    )
	
	project_include_directories += ['src/third_party/libisofs/libisofs']
	project_link_extra_args += ['-static']
    project_link_libraries += [libisofs]
endif

#
## Disc Image static library
#

libdiscimage = static_library(
    'discimage', [
        'src/disc_image/cdi.c',
        'src/disc_image/mds.c',
        'src/disc_image/disc_image.c',
        'src/disc_image/disc_image.h',
        'src/disc_image/private.h',
        'src/disc_image/edc/libedc.c',
        'src/disc_image/edc/patch.c',
        'src/disc_image/edc/edc_ecc.c',
    ]
)

project_link_libraries += [libdiscimage]

#
## Final Executable
#

executable(
    meson.project_name(), 
	project_sources,
    dependencies: project_deps,
	link_args: project_link_extra_args,	
    link_with: project_link_libraries,
	include_directories: project_include_directories,
    install: is_install_required
)
