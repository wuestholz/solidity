#!/usr/bin/env bash

#------------------------------------------------------------------------------
# Bash script to run solc-verify tests.
#
# The documentation for solidity is hosted at:
#
#     https://solidity.readthedocs.org
#
# ------------------------------------------------------------------------------
# This file is part of solidity.
#
# solidity is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# solidity is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with solidity.  If not, see <http://www.gnu.org/licenses/>
#
# (c) 2016 solidity contributors.
#------------------------------------------------------------------------------

## GLOBAL VARIABLES

REPO_ROOT=$(cd $(dirname "$0")/../.. && pwd)
SOLCVERIFY_TESTS="test/solc-verify"
SOLCVERIFY="$REPO_ROOT/build/solc/solc-verify.py"

## COLORS
RED=
GREEN=
BLACK=
if test -t 1 ; then
  RED=$(tput setaf 1)
  GREEN=$(tput setaf 2)
  BLACK=$(tput sgr0)
fi

# Time format
TIMEFORMAT="%U"

## Keep track of failed tests
FAIL=0
PASS=0
FAILED_TESTS=()

## Success/fail printout
function reportError() {
    test_name=$1
    elapsed=$2
    message=$3
    echo -n "$test_name "
    echo -n $RED
    echo FAIL [$elapsed s]
	echo -n $BLACK
    FAIL=$((FAIL+1))
    FAILED_TESTS+=("$test_name"$'\n'"$message")
}
function reportSuccess() {
    test_name=$1
    elapsed=$2
    message=$3
    echo -n "$test_name "
    echo -n $GREEN
    echo PASS [$elapsed s]
    echo -n $BLACK
    PASS=$((PASS+1))
}

# Temp files
OUT_PATH=`mktemp`
TIME_PATH=`mktemp`

# Run SOLC-VERIFY and check result based on file, arguments and expected output
function solcverify_check()
{
    filename="${1}"
    solcverify_args="${2}"
    out_expected="${3}"
   
    # Test id
    test_string="$filename"
    [[ !  -z  $solcverify_args  ]] && test_string="$test_string [ $solcverify_args ]"

    # Run it
    (time "$SOLCVERIFY" "${filename}" ${solcverify_args} >& $OUT_PATH) >& $TIME_PATH
    elapsed=$(cat $TIME_PATH)
    
    # Check output
    out_diff=$(diff -w $out_expected $OUT_PATH)
    if [ $? -ne 0 ]
    then
        reportError "$test_string" "$elapsed" "$out_diff"
    else
        reportSuccess "$test_string" "$elapsed"
    fi
}

# Run the tests
cd $REPO_ROOT
for filename in $SOLCVERIFY_TESTS/*.sol; do
    # Flags to use
    flags=""
    if [ -f "$filename.flags" ]; then
        flags=$(<"$filename.flags")
    fi
    # Run the test
	solcverify_check "$filename" "$flags" "$filename.gold" 
done

# Remove temps
rm -f $OUT_PATH $TIME_PATH 

# Print the details of the failed tests
if [ $FAIL -eq 0 ]
then
    exit 0
else
	for i in "${!FAILED_TESTS[@]}"; do echo "$((i+1)). ${FAILED_TESTS[$i]}"; done
    exit 1
fi

