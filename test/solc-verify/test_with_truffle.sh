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
SOLC_BIN="$REPO_ROOT/build/solc"

source ~/.nvm/nvm.sh
nvm use node

cd $REPO_ROOT/$SOLCVERIFY_TESTS

# Setup the truffle project
rm -Rf truffle
pushd .
mkdir truffle
cd truffle
truffle init
popd 
cp truffle-config.js truffle

# Copy the contracts to the truffle
CONTRACTS_WITH_MAIN=`grep -l verifier_main *.sol`
cp $CONTRACTS_WITH_MAIN truffle/contracts/

# Add to migrations
DEPLOY=truffle/migrations/2_deploy_contracts.js
(
for c in $CONTRACTS_WITH_MAIN
do 
  c_basename=`basename $c`
  c_name=${c_basename%.*}
  echo "var $c_name = artifacts.require('./$c_basename');"
done
echo
echo "module.exports = function(deployer) {"
for c in $CONTRACTS_WITH_MAIN
do 
  c_basename=`basename $c`
  c_name=${c_basename%.*}
  echo "  deployer.deploy($c_name);"
done
echo "};"
) > $DEPLOY

# Add to tests
TEST=truffle/test/All.js
(
for c in $CONTRACTS_WITH_MAIN
do 
  c_basename=`basename $c`
  c_name=${c_basename%.*}
  echo "var $c_name = artifacts.require('$c_name');"
done
echo
echo "contract('All', function(accounts) {"
echo "  var contract"
echo 
for c in $CONTRACTS_WITH_MAIN
do 
  c_basename=`basename $c`
  c_name=${c_basename%.*}   
  echo "  it('$c_name', function() {"
  echo "    return $c_name.deployed().then(function(instance) {"
  echo "      return instance.__verifier_main();"
  echo "    });"
  echo "  });"
done
echo "});"
) > $TEST

# Make sure to use the compiled SOLC
export PATH="$SOLC_BIN:$PATH"

# Now actually test
cd truffle
truffle test