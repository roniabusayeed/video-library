# Video Library
A video encoding and decoding library built on top of [FFmpeg](https://www.ffmpeg.org/).

## Dependencies
1. [FFmpeg](https://www.ffmpeg.org/) (version: 7.0). 

#### Installing `FFmpeg`
If you're on macOS and have [Homebrew](https://brew.sh/) installed on your system, you can simply run `brew install ffmpeg` to install the `ffmpeg` library. If you're on Linux, see if your distribution's package manager has FFmpeg available by running `sudo apt-get install ffmpeg` (for Debian-based distributions) or `sudo dnf install ffmpeg` (for Fedora-based distributions). For other Linux distributions, you may need to follow the installation instructions on the [official FFmpeg website](https://www.ffmpeg.org/download.html).

**Note for Linux users:** Ensure that `pkg-config` can locate the FFmpeg libraries. You can verify this by running the following command: `pkg-config --libs libavformat libavcodec libswscale`

## Build

1. Clone the Repository. (`git clone <project-url> [path-to-destination]`)

2. Navigate to the Project Root Directory. (`cd [path-to-project-root]`)

3. Run the Build Script. (`./build.sh`)

You can find the the compiled static library `libvideo.a` and shared library `libvideo.dylib` (if you're on macOS) or `libvideo.so` (if you're on Linux) inside `build/` directory under project root directory.


## Usage
There are several ways you can add this library to your projects.
#### Using As a Precompiled Library

Add the `include` directory located under the project root directory to your compiler's include path, and link against one of the compiled libraries (`libvideo.a`, `libvideo.so`, or `libvideo.dylib` depending on your platform).

Please note that detailed instructions on how to compile programs by linking against precompiled libraries are beyond the scope of this documentation.

Additionally, when using this library in your applications, ensure you link against the following FFmpeg libraries to avoid linker errors:

1. `libavformat`
2. `libavcodec`
3. `libswscale`

#### Using As a Meson Subproject
If your project's build system is [Meson](https://mesonbuild.com/), then you can simply add this entire project as a subproject to your own [Meson](https://mesonbuild.com/) project since this library is also developed as a [Meson](https://mesonbuild.com/) project.

Please refer to [Meson Subprojects](https://mesonbuild.com/Subprojects.html) documentation for more information.

## Code Example
```C++
#include <iostream>

#include <video-decoder.h>
#include <video-encoder.h>

int main() {
    VideoDecoder video_decoder("input.mkv");

    VideoEncoder video_encoder(
        "output.mp4",
        video_decoder.getHeight(),
        video_decoder.getWidth(),
        video_decoder.getFPS(),
        video_decoder.getBitrate()
    );

    std::cout << video_decoder.getWidth() << std::endl;
    std::cout << video_decoder.getHeight() << std::endl;
    std::cout << video_decoder.getFPS() << std::endl;
    std::cout << video_decoder.getBitrate() << std::endl;
    std::cout << video_decoder.getTotalDuration() << std::endl;
    
    int64_t total_duration = video_decoder.getTotalDuration();
    video_decoder.seekToTimestamp(total_duration * 0.5);

    uint8_t* rgb_buffer = new uint8_t[video_decoder.getHeight() * video_decoder.getWidth() * 3];
    int64_t pts = 0;

    while (pts < 0.55 * total_duration) {
        video_decoder.getNextFrame(rgb_buffer, &pts);
        video_encoder.encodeFrame(rgb_buffer, video_decoder.getWidth(), video_decoder.getHeight());
    }
    
    video_encoder.finalize();
    delete[] rgb_buffer;
} 
```