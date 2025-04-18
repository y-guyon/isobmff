# ISO Base Media File Format (ISOBMFF)

This repository is the official reference implementation of the ISO Base Media File Format.

The ISO base media file format is published by ISO as part 12 of the MPEG-4 specifications, ISO/IEC 14496-12. As such, it implements and conforms to part of MPEG-4.

This part of MPEG-4 is used heavily by standards other than MPEG-4, and this reference software is often used by the reference software for those other standards, but still provides, in those contexts, an implementation "claiming conformance to MPEG-4".

> 📢 Pull Requests are welcome but must also be submitted as formal contributions to MPEG. Please separate changes to the build system and core features into separate pull requests when possible.

### Documentation

- **API Reference**: [gh-pages](https://mpeggroup.github.io/isobmff)
- **Usage Examples & Legacy API Docs**: See the [Wiki](https://github.com/MPEGGroup/isobmff/wiki)

To build documentation locally with [Doxygen](https://www.doxygen.nl):

``` sh
doxygen Doxyfile
```

## Development

This project uses [CMake](https://cmake.org/) to build the software. While the repository contains legacy project files for various IDEs (e.g., Visual Studio, Xcode), those are no longer maintained and are preserved only for compatibility with other MPEG-related tools.

This repository includes:

- `libisomediafile`: a core library implementing ISO Base Media File Format
- Several command-line tools for reading, writing, or converting ISOBMFF-based files


### 🚀 Compiling

Example of commands to build the entire toolset on a Linux platform.

``` sh
git clone https://github.com/MPEGGroup/isobmff.git
cd isobmff
mkdir build && cd build
cmake ..
make
```

#### ⚙️ Optional CMake Configuration Flags

| Option                            | Default | Description                                                                 |
|-----------------------------------|---------|-----------------------------------------------------------------------------|
| `LIBISOMEDIAFILE_STRICT_WARNINGS` | `ON`    | Treats all compiler warnings as errors. Set to `OFF` to disable.            |
| `SET_CUSTOM_OUTPUT_DIRS`          | `ON`    | Places output binaries in `bin/` and libraries in `lib/`. Set to `OFF` to allow custom build layout. |

Example:

```sh
cmake -DLIBISOMEDIAFILE_STRICT_WARNINGS=OFF -DSET_CUSTOM_OUTPUT_DIRS=OFF -DCMAKE_BUILD_TYPE=Debug ..
```

### Cross-Platform Support

CMake supports multiple generators and toolchains:

``` sh
cmake -G "Visual Studio 16 2019" -A ARM64
```

For more options, refer to the [CMake generators documentation](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html).

> Many IDEs (e.g., Visual Studio, CLion) can open and parse CMakeLists.txt directly without manual configuration.

### Building Individual Components

If you are only interested in certain tools, you can build them individually.

For instance, the `libisomediafile` can be built using `make libisomediafile` when using Unix Makefile.

For a complete list, please refer to the generated build scripts, for instance with Unix Makefile:

``` sh
$ make help
The following are some of the valid targets for this Makefile:
... all (the default if no target is provided)
... clean
... depend
... rebuild_cache
... edit_cache
... libuniDrcBitstreamDecoderLib
... libwavIO
... libreadonlybitbuf
... libwriteonlybitbuf
... TLibDecoder
... TLibCommon
... libisomediafile
... makeAudioMovieSample
... playAudioMovieSample
... DRC_to_MP4
... MP4_to_DRC
... hevc_muxer
... hevc_demuxer
... hevc_extractors
... protectAudioMovie
... libisoiff
... isoiff_tool
... WAV_to_MP4
... MP4_to_WAV
```

### Code Formatting (clang-format)

We use clang-format to enforce consistent coding style in the libisomediafile library.

To format all `.c` and `.h` files under IsoLib/libisomediafile, run:

```sh
find IsoLib/libisomediafile -name "*.h" -o -name "*.cpp" -o -name "*.c" | xargs clang-format -style=file -i
```

> 💡 Make sure clang-format is installed and available in your `PATH`.

The formatting rules are defined in the project's [.clang-format](./.clang-format) file. Contributions to `libisomediafile` should follow this formatting style.
