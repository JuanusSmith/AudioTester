# AudioTester

AudioTester is a small Windows tool that checks **MP3, Ogg Vorbis, FLAC and WavPack** files for errors: corrupt data, truncated files, lost sync and failed CRC or MD5 checks. Drop files or folders onto the window and it tests them in parallel, one worker thread per CPU core, then lists every file that failed (with the reason) and every file that passed.

AudioTester was written by **James Chapman** ([vuplayer.com](http://www.vuplayer.com)) and released under the MIT license. Version 1.7 came out in 2015. This repository is an **unofficial update of 1.7** with bug fixes, current codec libraries and a Visual Studio 2026 build. All credit for the original program goes to James Chapman. His original readme is included unchanged as [ORIGINAL_README.txt](ORIGINAL_README.txt).

## Usage

- Drop files or folders onto the window. Folders are scanned recursively.
- Or start it from the command line with one or more files or folders:

  ```
  AudioTester.exe "D:\Music\Album 1" "D:\Music\track.flac"
  ```

  If AudioTester is already open, a new launch hands its files to the open window.
- Press **Stop** to cancel the files still waiting in the queue.
- Select the results with **Ctrl+A** and copy them with **Ctrl+C**.
- The system menu (click the window icon) has **About AudioTester**, which shows the codec library versions.

## What changed in 1.7.1 (2026)

### Bug fixes

| Area | Problem in 1.7 | Fix |
|---|---|---|
| FLAC | Valid files whose STREAMINFO block stores a total sample count of 0 ("unknown", common with streamed or piped encodes) were failed with `EXTRA SAMPLES`. | The sample-count check is skipped when the total is unknown. |
| FLAC | An empty or non-FLAC `.flac` file was reported as `MD5_MISMATCH`. | Reported as `NO FLAC STREAM FOUND`. |
| MP3 | The ID3v2 header was read as signed `char`, so the tag-size checks never worked and size bytes ≥ 0x80 produced a wrong size. | Read as `unsigned char`, so malformed tags are reported as `BAD ID3v2 TAG`. |
| MP3 | Re-syncing after an error called itself recursively, up to about 65,000 levels deep on junk data (stack overflow risk). | Rewritten as a loop. |
| Ogg, FLAC, WavPack | Some decoder errors returned failure without setting the error text, so garbage memory was displayed. | Error buffers are initialised, and unknown error codes are reported by number. |
| WavPack | A file that couldn't be opened crashed the app (`fread` on a null file handle). | Reported as a failure instead. |
| Network paths | Every path got a `\\?\` prefix, which breaks `\\server\share` paths: **a network folder found 0 files.** Relative command-line paths broke the same way. | Paths are resolved to full paths first, and UNC paths use the correct `\\?\UNC\` form. |
| Single instance | A second launch exited without passing on its files, and only the first command-line argument was used. | All arguments are used, and a second launch forwards its files to the open window. |
| Queue | A race in the work queue could leave newly added files waiting until the next drop. | The "files pending" event is reset while the queue lock is held. |
| Results order | Results were sorted by raw character codes, so lowercase names (e.g. `blink-182`) came after all uppercase ones, and track 10 came before track 2. | Sorted the way Explorer does: case-insensitive, with numbers in numeric order. |
| Large files | File sizes and positions were 32-bit, so files over 2 GB failed. | 64-bit throughout (the Vorbis library's own tell callback is still 32-bit). |
| Folder scan | Skipped any folder starting with `.`, and followed junctions and symlinks (possible endless loop). | Skips only `.` and `..`, and does not follow junctions or symlinks. |
| 64-bit build | `%d` was used to print `size_t` values (undefined behaviour on x64). | Uses `%zu`. |

### Performance and UI

- Choosing a decoder no longer holds the queue lock, so adding files or pressing Stop doesn't wait behind a big file load. Whole-file loads into memory still happen one at a time, as the original intended, to avoid disk thrashing, but they now have their own lock.
- In-memory loading is capped at 256 MB per file. Larger files are read with a buffered stream, which keeps memory use reasonable with many threads.
- Worker threads are no longer pinned to specific CPU cores, which works poorly on modern CPUs with mixed performance and efficiency cores.
- The app is DPI-aware and uses the system UI font, so it is no longer tiny or blurry on high-resolution screens.
- The 32-bit build is large-address-aware.

### Libraries and build

- The codec libraries are updated from 2014 versions (several releases since then fixed security issues triggered by crafted files):

  | Library | 1.7 | 1.7.1 |
  |---|---|---|
  | libogg | 1.3.2 | 1.3.6 |
  | libvorbis | 1.3.4 | 1.3.7 |
  | libFLAC | 1.3.1 | 1.5.0 |
  | WavPack | 4.70 | 5.9.0 |

- The libraries come from [vcpkg](https://vcpkg.io), with versions pinned in `vcpkg.json` and `vcpkg-configuration.json`. They are linked statically, so the exe needs no DLLs.
- The project builds with Visual Studio 2026 (v145 toolset) at warning level 4, for both x64 and Win32.
- The About box reads library versions from the libraries at run time instead of hardcoding them.

### Tests

The new `tests/` folder holds:

- **`make_test_files.cpp`**: writes known-good files for every format (FLAC with and without a stored length, Ogg Vorbis, WavPack with MD5, and MP3 with and without CRCs or an ID3v2 tag).
- **`make_damaged_files.py`**: makes damaged copies (truncated, flipped bytes, a broken frame header, damaged CRC-protected data, a malformed ID3v2 tag, junk and empty files) and an `expected.txt` listing the right result for each file.
- **`decoder_test.cpp`**: runs each file through the same decoder loop as the app, in both in-memory and buffered read modes.
- **`gui_test.ps1`**: starts the real app window, reads its results and checks them against `expected.txt`. It also checks that a second launch with a relative path forwards its file to the open window. Use `-Data` to point it at a network path.

All 25 test files give the expected result on both the x64 and Win32 builds. On the same files the original 1.7 exe wrongly fails the valid FLAC with an unknown length, ignores the second launch, and finds no files at all on a network path.

## Building

See [BUILDING.txt](BUILDING.txt). In short: Visual Studio 2026 with the C++ desktop workload, then

```
vcpkg install --triplet x64-windows-static --x-install-root=vcpkg_installed\x64
vcpkg install --triplet x86-windows-static --x-install-root=vcpkg_installed\x86
msbuild AudioTester.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild AudioTester.vcxproj /p:Configuration=Release /p:Platform=Win32
```

The output is `x64\Release\AudioTester.exe` and `Release\AudioTester.exe`. This repository contains source only, with no prebuilt exes.

## Known limitations

- MP3 checking is structural: frame sync, frame lengths, truncation, tags and (where present) frame CRCs. The audio itself is not decoded, so damage inside an MP3 without CRCs can go unnoticed.
- MP3 files with free-format bitrates are reported as `UNRECOGNISED FORMAT`.
- Ogg Opus, M4A/AAC/ALAC, WAV/AIFF and APE are not supported.
- Results can only be copied from the window, not saved to a file.

## Version history

**1.7.1** (unofficial, 17 October 2026): bug fixes, updated codec libraries, VS2026 build and tests, as described above.

Earlier versions (1.0 to 1.7, 2007 to 2015) are listed in [ORIGINAL_README.txt](ORIGINAL_README.txt).

## License

MIT. See [LICENSE](LICENSE). The original copyright belongs to James Chapman (2015). The codec libraries are separate works under their own BSD-style licenses.
