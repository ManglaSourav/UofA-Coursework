"""
Author: Edwin Renck
Class: CSc 452, Spring 2022
Purpose: Simulate different memory paging algorithms
"""

import sys
import pageTable
from os.path import exists

def main():
    if (len(sys.argv) != 6):
        print("Wrong number of arguments, please use:")
        print("vmsim.py -n <numframes> -a <opt|clock|lru|nfu> <tracefile>")
        sys.exit(-1)

    frames, algo, file = readArguments()

    pg = readFile(file, frames, algo)

    printResults(algo, frames, pg)


def printResults(algo, frames, pg):
    """
    Prints the results of our algorithm
    Args:
        algo -- The algorithm chosen
        dic -- the dictionary holding our access types
    Returns:
        -- None
    """
    print("Algorithm:", algo)
    print("Number of frames:\t ", frames)
    print("Total memory accesses:\t ", pg.readCount + pg.writeCount)
    print("Total page faults:\t ", pg.faultCount)
    print("Total writes to disk:\t ", pg.writeToDiskCount)
    print("Total size of page table:", 2**21, "bytes")

def readFile(file, frames, algo):
    """
    Reads through the file line by line, parsing valid lines and simulating the algorithm
    Returns:
        -- A dictionary mapping access types
    """
    f = open(file)
    pg = pageTable.PageTable(frames, algo)
    for line in f.readlines():
        args = line.split()
        instruction = args[0]
        # Invalid line
        if (len(args) < 2):
            continue
        if (instruction in ["I", "S", "L", "M"]):
            args = args[1].split(",")
            # Not enough memory, size arguments
            if (len(args) < 2):
                continue
            if (not args[1].isdigit()):
                continue
            pg.instruction(instruction, args[0])

    f.close()
    return pg
    

def readArguments():
    """
    Reads in the command line arguments and checks that all values are valid.
    Returns:
        -- The numbes of frames, the algorithm chosen and the path of the file to parse
    """
    frames = 0
    algo = ""
    file = ""

    count = 1

    while (count < 6):
        if (sys.argv[count] == '-n'):
            frames = int(sys.argv[count + 1])
            count += 2
        elif (sys.argv[count] == '-a'):
            algo = sys.argv[count + 1]
            if (algo not in ["opt","clock","lru","nfu"]):
                print("invalid algo, select from <opt,clock,lru,nfu>")
                sys.exit(-1)
            count += 2
        else:
            file = sys.argv[count]
            if (not exists(file)):
                print("File not found")
                sys.exit(-1)
            count += 1


    return (frames, algo, file)



if __name__ == "__main__":
    main()
