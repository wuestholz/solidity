import argparse
import re
import subprocess

solc = "~/Workspace/solidity-sri/build/solc/solc"
boogie = "~/Workspace/boogie/Binaries/Boogie.exe"

parser = argparse.ArgumentParser(description='Verify Solidity smart contracts.')
parser.add_argument('file', metavar='f', type=str, help='Path to the input file')
args = parser.parse_args()

solFile = args.file
bplFile = solFile + ".bpl"

convertCommand = solc + " --boogie " + solFile + " -o ./ --overwrite"
subprocess.check_output(convertCommand, shell = True)

verifyCommand = "mono " + boogie + " " + bplFile + " /nologo /doModSetAnalysis /errorTrace:0"
output = subprocess.check_output(verifyCommand, shell = True)
outputLines = list(filter(None, output.decode("utf-8").split("\n")))

for line, nextLine in zip(outputLines, outputLines[1:]):
    errFileLineCol = line.split(":")[0]
    errFile = errFileLineCol[:errFileLineCol.rfind("(")]
    errLineNo = int(errFileLineCol[errFileLineCol.rfind("(")+1:errFileLineCol.rfind(",")]) - 1
    errLine = open(errFile).readlines()[errLineNo]
    if "This assertion might not hold." in line:
        print(re.search("{:sourceloc ([^}]*)}", errLine).group(1) + ": " + re.search("{:message \"([^}]*)\"}", errLine).group(1))
    if "A postcondition might not hold on this return path." in line:
        nextErrFileLineCol = nextLine.split(":")[0]
        nextErrFile = nextErrFileLineCol[:nextErrFileLineCol.rfind("(")]
        nextErrLineNo = int(nextErrFileLineCol[nextErrFileLineCol.rfind("(")+1:nextErrFileLineCol.rfind(",")]) - 1
        nextErrLine = open(nextErrFile).readlines()[nextErrLineNo]
        print(re.search("{:sourceloc ([^}]*)}", nextErrLine).group(1) + ": " + re.search("{:message \"([^}]*)\"}", nextErrLine).group(1))
    if "A precondition for this call might not hold." in line:
        nextErrFileLineCol = nextLine.split(":")[0]
        nextErrFile = nextErrFileLineCol[:nextErrFileLineCol.rfind("(")]
        nextErrLineNo = int(nextErrFileLineCol[nextErrFileLineCol.rfind("(")+1:nextErrFileLineCol.rfind(",")]) - 1
        errLinePrev = open(errFile).readlines()[errLineNo-1]
        nextErrLine = open(nextErrFile).readlines()[nextErrLineNo]
        print(re.search("{:sourceloc ([^}]*)}", errLinePrev).group(1) + ": " + re.search("{:message \"([^}]*)\"}", nextErrLine).group(1))