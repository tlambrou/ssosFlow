# Ardour Fork Build Notes

Issue: #1

## Repository State

This workspace is a named GitHub fork:

- Local path: `/Users/Tassos/Documents/ssosFlow`
- GitHub repo: `https://github.com/tlambrou/ssosFlow`
- Upstream remote: `https://github.com/Ardour/ardour.git`
- Branch policy:
  - `main`: human-reviewed stable branch for this fork.
  - `staging`: AI-assisted integration branch.
  - `feature/codex-1-reactive-performance-research`: current Phase 0/1 issue branch.
  - `master`: retained as an Ardour upstream mirror/reference branch.

Current checkout was fetched shallowly for research. Before building, convert it to a full clone with tags:

```bash
git fetch --unshallow --tags upstream
git fetch --tags origin
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

## Configure Command Sketch

For macOS research builds, start conservatively:

```bash
python3 ./waf configure --with-backends=coreaudio --cxx17 --compile-database --test
```

If the dependency stack is installed outside system paths, use:

```bash
python3 ./waf configure --with-backends=coreaudio --cxx17 --compile-database --test --depstack-root="$HOME"
```

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

## Build and Test Sketch

Build:

```bash
python3 ./waf build -j"$(sysctl -n hw.logicalcpu)"
```

Run libardour tests after configuring with `--test`:

```bash
cd libs/ardour
../../waf --targets=libardour-tests
./run-tests.sh
```

Note: `doc/unit_tests.txt` mentions using `doc/waft` for a neater test flow. Validate the current generated test runner after the first successful configure/build, because this research pass did not compile Ardour.

## Known Build Risks

- The checkout is shallow until `git fetch --unshallow --tags upstream` is run.
- `python` is not on PATH; use `python3`.
- macOS dependency stack may be missing.
- Full Ardour builds are large; first implementation work should add parser/unit tests before attempting full GUI validation.
- Avoid changing build/governance files unless a concrete issue requires it and the change is explicitly issue-scoped.
