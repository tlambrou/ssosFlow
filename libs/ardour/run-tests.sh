#!/bin/bash
#
# Run libardour test suite.
#

TOP=`dirname "$0"`/../..
. $TOP/build/gtk2_ardour/ardev_common_waf.sh
ARDOUR_LIBS_DIR=$TOP/build/libs/ardour

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
