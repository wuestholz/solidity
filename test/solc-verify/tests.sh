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

set -e

## GLOBAL VARIABLES

SOLCVERIFY_TESTS=$(cd $(dirname "$0") && pwd)
REPO_ROOT=$(cd $(dirname "$0")/../.. && pwd)
SOLCVERIFY="$REPO_ROOT/build/solc/solc-verify.py"

## FUNCTIONS

if [ "$CIRCLECI" ]
then
    function printTask() { echo "$(tput bold)$(tput setaf 2)$1$(tput setaf 7)"; }
    function printError() { echo "$(tput setaf 1)$1$(tput setaf 7)"; }
else
    function printTask() { echo "$(tput bold)$(tput setaf 2)$1$(tput sgr0)"; }
    function printError() { echo "$(tput setaf 1)$1$(tput sgr0)"; }
fi

# General helper function for testing SOLC-VERIFY behaviour, based on file name, compile opts, exit code, stdout and stderr.
# An failure is expected.
function test_solcverify_behaviour()
{
    local filename="${1}"
    local solcverify_args="${2}"
    local solcverify_stdin="${3}"
    local stdout_expected="${4}"
    local exit_code_expected="${5}"
    local stderr_expected="${6}"
    local stdout_path=`mktemp`
    local stderr_path=`mktemp`
    if [[ "$exit_code_expected" = "" ]]; then exit_code_expected="0"; fi

    set +e
    if [[ "$solc_stdin" = "" ]]
    then
        "$SOLCVERIFY" "${filename}" ${solc_args} 1>$stdout_path 2>$stderr_path
    else
        "$SOLCVERIFY" "${filename}" ${solc_args} <$solc_stdin 1>$stdout_path 2>$stderr_path
    fi
    exitCode=$?
    set -e

    if [[ $exitCode -ne "$exit_code_expected" ]]
    then
        printError "Incorrect exit code. Expected $exit_code_expected but got $exitCode."
        rm -f $stdout_path $stderr_path
        exit 1
    fi

    if [[ "$(cat $stdout_path)" != "${stdout_expected}" ]]
    then
        printError "Incorrect output on stdout received. Expected:"
        echo -e "${stdout_expected}"

        printError "But got:"
        cat $stdout_path
        printError "When running $SOLC ${filename} ${solc_args} <$solc_stdin"

        rm -f $stdout_path $stderr_path
        exit 1
    fi

    if [[ "$(cat $stderr_path)" != "${stderr_expected}" ]]
    then
        printError "Incorrect output on stderr received. Expected:"
        echo -e "${stderr_expected}"

        printError "But got:"
        cat $stderr_path
        printError "When running $SOLC ${filename} ${solc_args} <$solc_stdin"

        rm -f $stdout_path $stderr_path
        exit 1
    fi

    rm -f $stdout_path $stderr_path
}

for filename in $SOLCVERIFY_TESTS/*.sol; do
    flags=""
    if [ -f "$filename.flags" ]; then
        flags=$(<"$filename.flags")
    fi
	if [ -f "filename.gold" ]; then 
		gold=$(<"$filename.gold")
    fi
    exp="$sol.expected"
	test_solcverify_behaviour "$filename" "$flags" "" "$gold" 
done

cleanup
