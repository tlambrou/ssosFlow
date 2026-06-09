# Ardour Fork Build Notes

Issues: #1 for Phase 0/1 research, #3 for Phase 2 build setup.

## Repository State

This workspace is a named GitHub fork:

- Local path: `/Users/Tassos/Documents/ssosFlow`
- GitHub repo: `https://github.com/tlambrou/ssosFlow`
- Upstream remote: `https://github.com/Ardour/ardour.git`
- Branch policy:
  - `main`: human-reviewed stable branch for this fork.
  - `staging`: AI-assisted integration branch.
  - `feature/codex-3-ardour-build-setup`: current Phase 2 issue branch.
  - `master`: retained as an Ardour upstream mirror/reference branch.

The checkout was originally shallow. Phase 2 converted it to a full clone with tags:

```bash
git fetch --unshallow --tags upstream
git fetch --tags origin
```

Verified on 2026-06-09:

```bash
git rev-parse --is-shallow-repository
# false
git tag --list | wc -l
# 158
git describe --tags --always --dirty
# 9.7-35-gab961d3607 after the Phase 2 docs commit
```

Reason: Ardour's `README-GITHUB.txt` says GitHub release tarballs are not supported because the build depends on Git-derived version information.

## Python/Waf

On this machine, `./waf --help` failed because `python` was not found. `python3 ./waf --help` succeeded.

Use one of these before configuring:

```bash
python3 ./waf --help
```

or create a local shell alias/shim outside the repo:

```bash
alias python=python3
```

Do not commit a local Python shim to the repository.

## Homebrew Dependencies

Phase 2 verified this dependency set with arm64 Homebrew:

```bash
brew install \
  glibmm@2.66 libsndfile libarchive liblo taglib vamp-plugin-sdk rubberband \
  cppunit aubio jpeg-turbo pango cairomm@1.14 pangomm@2.46 lv2 lrdf \
  libwebsockets serd sord sratom lilv fftw libusb
```

Confirmed versions on 2026-06-09:

```text
glibmm-2.4 2.66.8
libarchive 3.8.7
libusb-1.0 1.0.30
fftw3f 3.3.11
lrdf 0.5.0
```

Notes:

- `libarchive` is keg-only for pkg-config discovery on this machine. Use `PKG_CONFIG_PATH=/opt/homebrew/opt/libarchive/lib/pkgconfig`.
- The raw `jpeglib.h` check needs `/opt/homebrew/include`.
- Homebrew `lrdf.pc` can point at a stale `raptor` Cellar path. Add `/opt/homebrew/opt/raptor/include/raptor2` to `CPPFLAGS`.
- Keep `/usr/local` Intel Homebrew libraries out of the effective pkg-config path. `fftw` and `libusb` both needed arm64 `/opt/homebrew` installs during Phase 2.

## Configure Command

Final verified configure command:

```bash
PKG_CONFIG_PATH=/opt/homebrew/opt/libarchive/lib/pkgconfig \
CPPFLAGS="-I/opt/homebrew/include -I/opt/homebrew/opt/libarchive/include -I/opt/homebrew/opt/raptor/include/raptor2" \
LDFLAGS="-L/opt/homebrew/lib -L/opt/homebrew/opt/libarchive/lib -L/opt/homebrew/opt/raptor/lib" \
python3 ./waf configure --with-backends=coreaudio --arm64 --cxx17 --compile-database --test
```

Result on 2026-06-09: configure succeeded. Important enabled items:

- CoreAudio/Midi backend
- Dummy backend
- Unit tests
- Lua command-line tool
- LV2 support/extensions/UI embedding
- VST3 support
- Mac VST support
- AudioUnits
- Mac arm64 architecture

Expected nonfatal configure notes:

- `cwiid.h` is missing, so Wiimote support is disabled.
- Waf still prints an SSE warning even with `--arm64`, while the final summary reports `Mac arm64 Architecture : True`.
- Homebrew emits a tap trust warning for `powershell/tap`; it is unrelated to this build.

Useful options confirmed by `python3 ./waf --help`:

- `--with-backends=coreaudio`
- `--arm64`
- `--cxx17`
- `--compile-database`
- `--test`
- `--run-tests`
- `--depstack-root=...`
- `--debug-symbols`
- `--strict`

## Build Command

Final verified build command:

```bash
PKG_CONFIG_PATH=/opt/homebrew/opt/libarchive/lib/pkgconfig \
CPPFLAGS="-I/opt/homebrew/include -I/opt/homebrew/opt/libarchive/include -I/opt/homebrew/opt/raptor/include/raptor2" \
LDFLAGS="-L/opt/homebrew/lib -L/opt/homebrew/opt/libarchive/lib -L/opt/homebrew/opt/raptor/lib" \
python3 ./waf build -j"$(sysctl -n hw.logicalcpu)"
```

Results on 2026-06-09:

- Full build completed successfully in 7m21s before the `libusb` cleanup.
- After installing arm64 `libusb`, configure succeeded again and incremental build completed successfully in 5m03s.
- The final link uses `/opt/homebrew/opt/libusb/lib/libusb-1.0.0.dylib`; the earlier `/usr/local/Cellar/libusb/... x86_64` warning is resolved.

## Source Patch Required

Darwin/Clang rejected generated GTK2 alias definitions with:

```text
aliases are not supported on darwin
```

Phase 2 fixed this by defining `DISABLE_VISIBILITY` only for Darwin builds in:

- `libs/tk/ydk/wscript`
- `libs/tk/ytk/wscript`

The failing source was generated alias code guarded by `#ifndef DISABLE_VISIBILITY`.

## Test and Smoke Results

Directly executing built test binaries is not reliable because several tests require fixture/env search paths. Use either the wrapper scripts or set these variables:

```bash
PBD_TEST_PATH="$PWD/libs/pbd/test"
MIDIPP_TEST_PATH="$PWD/share/patchfiles"
EVORAL_TEST_PATH="$PWD/libs/evoral/test/testdata"
```

Focused post-build results:

- `build/libs/midi++2/run-tests` passed with `MIDIPP_TEST_PATH`.
- `build/libs/evoral/run-tests` passed with `EVORAL_TEST_PATH`.
- `build/libs/temporal/run-tests` passed.
- `build/libs/audiographer/run-tests` passed.
- `build/libs/pbd/run-tests` still fails in `RWLockTest::run_thread_sequence_test` with `assertion failed - Expression: rl2.locked()`.
- `libs/ardour/run-tests.sh` sets the Ardour runtime environment and starts successfully, but segfaults during `LuaScriptTest::session_script_test` after earlier `AudioEngineTest`, `AutomationListPropertyTest`, `DSPLoadCalculatorTest`, and `FPUTest` checks pass.

Development wrapper smoke check:

```bash
gtk2_ardour/ardev --help
```

Result: exits 0 and prints Ardour usage without missing runtime-path warnings.

## Known Build Risks

- `python` is not on PATH; use `python3`.
- Many Homebrew bottles are built for a newer macOS version than Ardour's `-mmacosx-version-min=11.0`; this creates linker warnings but did not block the build.
- The `pbd` and `ardour` test runner failures above remain open verification risks for Phase 3.
- Full Ardour builds are large; first implementation work should add narrow parser/unit tests before attempting full GUI validation.
- Avoid changing build/governance files unless a concrete issue requires it and the change is explicitly issue-scoped.
