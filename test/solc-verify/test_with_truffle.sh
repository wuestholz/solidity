#!/bin/bash

# directory of the script
BOOGIE_TEST_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd $BOOGIE_TEST_DIR

if [ ! -d ".env" ]; then
  echo "We need truffle to run these tests"
  echo pip install nodeenv
  echo nodeenv --requirements=node-requirements.txt .env
  exit 1
fi

# Load up truffle
source .env/bin/activate

# Make the truffle project
if [ ! -d "truffle" ]; then
  pushd .
  mkdir truffle
  cd truffle
  truffle init
  popd 
fi

CONTRACTS_WITH_MAIN=`grep -l verifier_main $BOOGIE_TEST_DIR/*.sol`

# Copy contracts to truffle/contracts
for c in $CONTRACTS_WITH_MAIN
do
  cp $c truffle/contracts/
done

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

# Now actually test
cd truffle
truffle test