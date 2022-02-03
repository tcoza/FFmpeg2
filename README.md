FFmpeg README
=============

FFmpeg is a collection of libraries and tools to process multimedia content
such as audio, video, subtitles and related metadata.

## Libraries

* `libavcodec` provides implementation of a wider range of codecs.
* `libavformat` implements streaming protocols, container formats and basic I/O access.
* `libavutil` includes hashers, decompressors and miscellaneous utility functions.
* `libavfilter` provides means to alter decoded audio and video through a directed graph of connected filters.
* `libavdevice` provides an abstraction to access capture and playback devices.
* `libswresample` implements audio mixing and resampling routines.
* `libswscale` implements color conversion and scaling routines.

## Tools

* [ffmpeg](https://ffmpeg.org/ffmpeg.html) is a command line toolbox to
  manipulate, convert and stream multimedia content.
* [ffplay](https://ffmpeg.org/ffplay.html) is a minimalistic multimedia player.
* [ffprobe](https://ffmpeg.org/ffprobe.html) is a simple analysis tool to inspect
  multimedia content.
* Additional small tools such as `aviocat`, `ismindex` and `qt-faststart`.

## Documentation

The offline documentation is available in the **doc/** directory.

The online documentation is available in the main [website](https://ffmpeg.org)
and in the [wiki](https://trac.ffmpeg.org).

### Examples

Coding examples are available in the **doc/examples** directory.

## License

FFmpeg codebase is mainly LGPL-licensed with optional components licensed under
GPL. Please refer to the LICENSE file for detailed information.

## Contributing

### Mailing List

Patches should be submitted to the ffmpeg-devel mailing list using
`git format-patch` or `git send-email` as described in the 
[Developer Documentation](https://www.ffmpeg.org/developer.html#Contributing).

### Pull Requests

There is a new experimental way that allows submitting Pull Requests
and having them forwarded to the ffmpeg Mailing List.

Please submit your Pull Requests here: **https://github.com/ffstaging/FFmpeg**.

Then, follow the instructions from the automatic CodeBot response.
Besides the submission procedure, the
[Developer Documentation](https://www.ffmpeg.org/developer.html#Contributing)
applies in the same way like when submitting to the ML directly.
