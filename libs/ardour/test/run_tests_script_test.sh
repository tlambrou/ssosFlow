#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
RUNNER="$SCRIPT_DIR/../run-tests.sh"
REAL_HOME="${HOME:?}"

fail ()
{
	echo "FAIL: $*" >&2
	exit 1
}

make_fixture ()
{
	local fixture
	fixture=$(mktemp -d "${TMPDIR:-/tmp}/ardour-run-tests-script.XXXXXX")
	mkdir -p "$fixture/libs/ardour" "$fixture/build/gtk2_ardour" "$fixture/build/libs/ardour"

	cp "$RUNNER" "$fixture/libs/ardour/run-tests.sh"
	chmod +x "$fixture/libs/ardour/run-tests.sh"

	cat > "$fixture/build/gtk2_ardour/ardev_common_waf.sh" <<'EOF'
: "${TOP:?}"
EOF

	cat > "$fixture/build/libs/ardour/run-tests" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$HOME" > "${ARDOUR_CAPTURE_HOME:?}"
EOF
	chmod +x "$fixture/build/libs/ardour/run-tests"

	cat > "$fixture/build/libs/ardour/test_dummy" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$HOME" > "${ARDOUR_CAPTURE_HOME:?}"
EOF
	chmod +x "$fixture/build/libs/ardour/test_dummy"

	cat > "$fixture/waf" <<'EOF'
#!/usr/bin/env python3
import sys
sys.exit(0)
EOF
	chmod +x "$fixture/waf"

	printf '%s\n' "$fixture"
}

run_fixture ()
{
	local fixture=$1
	local capture=$2
	shift 2
	ARDOUR_CAPTURE_HOME="$capture" "$fixture/libs/ardour/run-tests.sh" "$@"
}

assert_isolated_home ()
{
	local captured=$1
	local mode=$2

	[ -n "$captured" ] || fail "$mode did not capture HOME"
	[ "$captured" != "$REAL_HOME" ] || fail "$mode used the real HOME"
	[ ! -d "$captured" ] || fail "$mode left temporary HOME behind: $captured"
}

fixture=$(make_fixture)
trap 'rm -rf "$fixture"' EXIT

capture="$fixture/full-home.txt"
run_fixture "$fixture" "$capture"
assert_isolated_home "$(cat "$capture")" "full run"

capture="$fixture/single-home.txt"
run_fixture "$fixture" "$capture" --single dummy
assert_isolated_home "$(cat "$capture")" "single run"

custom_home="$fixture/custom-home"
capture="$fixture/custom-home.txt"
ARDOUR_TEST_HOME="$custom_home" run_fixture "$fixture" "$capture"
[ "$(cat "$capture")" = "$custom_home" ] || fail "ARDOUR_TEST_HOME override was not used"
[ -d "$custom_home" ] || fail "ARDOUR_TEST_HOME override directory should be preserved"

capture="$fixture/real-home.txt"
ARDOUR_TEST_USE_REAL_HOME=1 run_fixture "$fixture" "$capture"
[ "$(cat "$capture")" = "$REAL_HOME" ] || fail "ARDOUR_TEST_USE_REAL_HOME did not preserve the real HOME"

echo "run-tests.sh HOME isolation checks passed"
