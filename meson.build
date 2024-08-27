project(
	'video',
	['c', 'cpp'],
	version: '1.0.0',
	default_options: [
		'c_std=c99',
		'cpp_std=c++20',
		'default_library=both'
	]
)

include_directories = include_directories('include')

sources = files(
	'src/video-encoder.cpp',
	'src/video-decoder.cpp'
)

# FFmpeg dependencies.
ffmpeg_lib_names = ['libavformat', 'libavcodec', 'libswscale']
ffmpeg_lib_deps = []
foreach ffmpeg_lib_name : ffmpeg_lib_names
	ffmpeg_lib_deps += dependency(ffmpeg_lib_name)
endforeach
ffmpeg_dep = declare_dependency(
	dependencies: ffmpeg_lib_deps
)

# Package all dependencies together.
dependencies = [
	ffmpeg_dep
]

# Create the library.
video_lib = library(
	'video',
	include_directories: include_directories,
	sources: sources,
	dependencies: ffmpeg_dep,
	install: true  # required to build current archive instead of thin archive.
)

# Expose dependency for using this project as a subproject.
video_dep = declare_dependency(
	include_directories: include_directories,
	link_with: video_lib,
	dependencies: ffmpeg_dep
)
