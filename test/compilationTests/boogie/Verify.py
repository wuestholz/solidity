#!/usr/bin/env python3

import argparse
import re
import subprocess
import os
import threading
import signal

def kill():
    print("Timeout while running verifier")
    os.killpg(os.getpgrp(), signal.SIGKILL)

def main():
    # Set up argument parser
    parser = argparse.ArgumentParser(description='Verify Solidity smart contracts.')
    parser.add_argument('file', type=str, help='Path to the input file')
    parser.add_argument('--bit-precise', action='store_true', help='Enable bit-precise verification')
    parser.add_argument("--solc", type=str, help="Solidity compiler to use (with boogie translator)", default="~/Workspace/solidity-sri/build/solc/solc")
    parser.add_argument("--boogie", type=str, help="Boogie binary to use", default="~/Workspace/boogie-yices/Binaries/Boogie.exe")
    parser.add_argument("--output", type=str, help="Output directory", default=".")
    parser.add_argument("--timeout", type=int, help="Timeout for running Boogie (in seconds)", default=30)
    parser.add_argument('--yices', action='store_true', help='Use Yices as SMT solver')
    parser.add_argument('--verbose', action='store_true', help='Print all output of the compiler and the verifier')
    args = parser.parse_args()

    solFile = args.file
    bplFile = args.output + "/" + os.path.basename(solFile) + ".bpl"

    # First convert .sol to .bpl
    solcArgs = " --boogie " + solFile + " -o " + args.output + " --overwrite" + (" --bit-precise" if args.bit_precise else "")
    convertCommand = args.solc + " " + solcArgs
    try:
        compilerOutput = subprocess.check_output(convertCommand, shell = True, stderr=subprocess.STDOUT)
        if args.verbose:
            compilerOutputStr = compilerOutput.decode("utf-8")
            print("----- Compiler output -----")
            print(compilerOutputStr)
            print("---------------------------")
    except subprocess.CalledProcessError as err:
        compilerOutputStr = err.output.decode("utf-8")
        print("Error while running compiler, details:")
        print(compilerOutputStr)
        return

    # Run timer
    timer = threading.Timer(args.timeout, kill)
    # Run verification, get result
    timer.start()
    boogieArgs = "/nologo /doModSetAnalysis /errorTrace:0" + (" /proverOpt:SOLVER=Yices2 /useArrayTheory" if args.yices else "")
    verifyCommand = "mono " + args.boogie + " " + bplFile + " " + boogieArgs
    verifierOutput = subprocess.check_output(verifyCommand, shell = True, stderr=subprocess.STDOUT)
    timer.cancel()

    verifierOutputStr = verifierOutput.decode("utf-8")
    if re.search("Boogie program verifier finished with", verifierOutputStr) == None:
        print("Error while running verifier, details:")
        print(verifierOutputStr)        
        return
    elif args.verbose:
        print("----- Verifier output -----")
        print(verifierOutputStr)
        print("---------------------------")

    # Map results back to .sol file
    outputLines = list(filter(None, verifierOutputStr.split("\n")))
    for outputLine, nextOutputLine in zip(outputLines, outputLines[1:]):
        if "This assertion might not hold." in outputLine:
            errLine = getRelatedLineFromBpl(outputLine, 0) # Info is in the current line
            print(getSourceLineAndCol(errLine) + ": " + getMessage(errLine))
        if "A postcondition might not hold on this return path." in outputLine:
            errLine = getRelatedLineFromBpl(nextOutputLine, 0) # Info is in the next line
            print(getSourceLineAndCol(errLine) + ": " + getMessage(errLine))
        if "A precondition for this call might not hold." in outputLine:
            errLine = getRelatedLineFromBpl(nextOutputLine, 0) # Message is in the next line
            errLinePrev = getRelatedLineFromBpl(outputLine, -1) # Location is in the line before
            print(getSourceLineAndCol(errLinePrev) + ": " + getMessage(errLine))
        if "Verification inconclusive" in outputLine:
            errLine = getRelatedLineFromBpl(outputLine, 0) # Info is in the current line
            print(getSourceLineAndCol(errLine) + ": Inconclusive result for function '" + getMessage(errLine) + "'")

    if (re.match("Boogie program verifier finished with \\d+ verified, 0 errors", outputLines[-1])):
        print("No errors found.")

# Gets the line related to an error in the output
def getRelatedLineFromBpl(outputLine, offset):
    # Errors have the format "filename(line,col): Message"
    errFileLineCol = outputLine.split(":")[0]
    errFile = errFileLineCol[:errFileLineCol.rfind("(")]
    errLineNo = int(errFileLineCol[errFileLineCol.rfind("(")+1:errFileLineCol.rfind(",")]) - 1
    return open(errFile).readlines()[errLineNo + offset]

# Gets the original (.sol) line and column number from an annotated line in the .bpl
def getSourceLineAndCol(line):
    match = re.search("{:sourceloc \"([^}]*)\", (\\d+), (\\d+)}", line)
    if match is None:
        return "[Could not trace back error location]"
    else:
        return match.group(1) + ", line " + match.group(2) + ", col " + match.group(3)

# Gets the message from an annotated line in the .bpl
def getMessage(line):
    match = re.search("{:message \"([^}]*)\"}", line)
    if match is None:
        return "[No message found for error]"
    else:
        return match.group(1)

if __name__== "__main__":
    main()