#!/bin/bash
#
# Run libardour test suite.
#

TOP=`dirname "$0"`/../..
. "$TOP/build/gtk2_ardour/ardev_common_waf.sh"
ARDOUR_LIBS_DIR=$TOP/build/libs/ardour
LIBARDOUR_DYLIB=$ARDOUR_LIBS_DIR/libardour.dylib

function waf_target_for_test_program {
	local test_name="$1"

	case "$test_name" in
		test_control_surfaces)
			echo "unit-test-control_surface"
			;;
		test_*)
			echo "unit-test-${test_name#test_}"
			;;
		*)
			return 1
			;;
	esac
}

function ensure_single_test_current {
	local test_program="$1"
	local test_name
	local waf_target

	test_name=`basename "$test_program"`
	waf_target=`waf_target_for_test_program "$test_name"` || return 1

	if [ "$LIBARDOUR_DYLIB" -nt "$test_program" ]; then
		echo "Removing stale $test_program before rebuilding $waf_target..."
		rm -f "$test_program"
	else
		echo "Rebuilding $waf_target before running $test_name..."
	fi

	python3 "$TOP/waf" build --target="$waf_target" || return 1

	if [ ! -x "$test_program" ]; then
		echo "Expected rebuilt test executable '$test_program' was not created" >&2
		return 1
	fi

	if [ "$LIBARDOUR_DYLIB" -nt "$test_program" ]; then
		echo "Refusing to run stale '$test_program'; it is older than '$LIBARDOUR_DYLIB' after rebuilding $waf_target" >&2
		echo "Try removing the stale test executable or rebuilding from a clean tree." >&2
		return 1
	fi
}

if [ "$1" == "--single" ] || [ "$2" == "--single" ]; then
        if [ "$1" == "--single" ]; then
	        TESTS="test_*$2*"
        elif [ "$2" == "--single" ]; then
	        TESTS="test_*$3*"
	else
                TESTS='test_*'
        fi
	status=0
	ran_any=0
	while IFS= read -r test_program; do
		if [ ! -x "$test_program" ]; then
			continue
		fi

		ran_any=1
		if ! ensure_single_test_current "$test_program"; then
			status=1
			continue
		fi

		echo "Running $test_program..."
		if [ "$1" == "--debug" ]; then
			gdb "$test_program"
		elif [ "$1" == "--valgrind" ]; then
			valgrind "$test_program"
	        else
			"$test_program"
	        fi
		if [ "$?" != "0" ]; then
			status=1
		fi
	done < <(find "$ARDOUR_LIBS_DIR" -name "$TESTS" -type f | sort)
	if [ "$ran_any" == "0" ]; then
		echo "No executable libardour tests matched '$TESTS'" >&2
		exit 1
	fi
	exit "$status"
else
        if [ "$1" == "--debug" ]; then
                gdb $ARDOUR_LIBS_DIR/run-tests
        elif [ "$1" == "--valgrind" ]; then
                valgrind $ARDOUR_LIBS_DIR/run-tests
        else
                $ARDOUR_LIBS_DIR/run-tests $*
        fi
fi
