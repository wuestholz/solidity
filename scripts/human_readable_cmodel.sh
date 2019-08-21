#!/usr/bin/env bash
set -e

#
# Helper script to transpile, and then format a single Solidity contract.
#
# This script will produce two artifacts, cmodel.h and cmodel.c, as are
# produced by `solc <input_file> --c-model --output-dir=<output_dir>`. If errors
# or warnings are produced, they will be written to cmodel.warning.
#
# Usage: <path_to_script> <input_file> [output_dir]
# * input_file: positional argument; the Solidity contract to transpile.
# * output_dir: optional positional argument; directory to write cmodel.{h,c}.
#

# Reads in commandline arguments.
OUTPUT_DIR="`pwd`"
SOLC_PATH="`pwd`/build/solc/solc"

# Ensures a file was passed, and that the file is valid.
if [[ $# -lt 1 ]]; then
	echo >&2 "Error: An input file is as the first argument required."
	exit 1;
fi
SRC_FILE="$1"
if [[ ! -f "$SRC_FILE" ]]; then
	echo >&2 "Error: Invalid source file provided: ${SRC_FILE}"
	exit 1;
fi

# If an output directory was given, uses it.
if [[ $# -gt 1 ]]; then
	OUTPUT_DIR="$2"
fi
if [[ ! "$OUTPUT_DIR" || ! -d "$OUTPUT_DIR" ]]; then
	echo >&2 "Error: Invalid output directory: ${OUTPUT_DIR}."
	exit 1;
fi

# Ensures clang-format is accessible.
command -v clang-format > /dev/null 2>&1 || {
	echo >&2 "Error: Failed to locate clang-format. clang-format is required for automatic output formatting.";
	exit 1;
}

# Ensures build/solc/solc is accessible.
if [[ ! "$SOLC_PATH" || ! -x "$SOLC_PATH" ]]; then
	echo >&2 "Error: Unable to locate solc."
	echo >&2 "Searching at: $SOLC_PATH";
	exit 1;
fi

# Acquires a temporary directory.
TMP_DIR=`mktemp -d`
if [[ ! "$TMP_DIR" || ! -d "$TMP_DIR" ]]; then
	echo >&2 "Error: Unable to acquire temporary directory."
	exit 1;
fi
function cleanup_tmp_dir { rm -rf "$TMP_DIR"; }
trap cleanup_tmp_dir EXIT

# Writes output to temporary directory.
${SOLC_PATH} ${SRC_FILE} --c-model --output-dir=${TMP_DIR} 2> "${OUTPUT_DIR}/cmodel.warning"
clang-format "${TMP_DIR}/cmodel.h" > "${OUTPUT_DIR}/cmodel.h"
clang-format "${TMP_DIR}/cmodel.c" > "${OUTPUT_DIR}/cmodel.c"
clang-format "${TMP_DIR}/primitive.h" > "${OUTPUT_DIR}/primitive.h"
