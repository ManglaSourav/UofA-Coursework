class PageTable:
    """
    Represents a page table and some of the instructions that it can follow
    """
    def __init__(self, frames, algo):
        self.table = [Entry(frames, algo) for n in range(2**19)]
        self.faultCount = 0
        self.writeCount = 0
        self.writeToDiskCount = 0
        self.readCount = 0

    def instruction(self, inst, fullAddress):
        hexVal = self.parseAddress(fullAddress)
        entry = self.table[hexVal]
        self.parseInstruction(inst)
        if (entry.init):
            error = entry.add(1)
            if (error):
                self.writeCount += 1
        else:
            self.pageFault(entry, hexVal)

    def parseAddress(self, fullAddress):
        hexVal = int(fullAddress, 16) >> 13
        return hexVal

    def parseInstruction(self, instruction):
        if (instruction in ['I', 'L', 'M']):
            self.readCount += 1
        if (instruction in ['S', 'M']):
            self.writeCount += 1

    def pageFault(self, entry, val):
        self.faultCount += 1
        if (not entry.init):
            entry.init = True
        elif (len(entry.frame) == entry.maxNum):
            self.writeToDiskCount += 1
        entry.add(val)
            

        
class Entry:
    """
    Represents a single frame entry
    """
    def __init__(self, numberOfFrames, algo):
        self.maxNum = numberOfFrames
        self.frame = []
        self.init = False
        self.algo =  algo

    def add(self, val):
        if (len(self.frame) < self.maxNum):
            self.frame.append(val)
            return False
        else:
            self.remove()
            self.frame.pop()
            self.frame.append(val)
            return True


    def remove(self):
        if (self.algo == "opt"):
            self.removeOpt()
        elif (self.algo == "clock"):
            self.removeClock()
        elif (self.algo == "lru"):
            self.removeLru()
        elif (self.algo == "nfu"):
            self.removeNfu()

    def removeOpt(self):
        pass
    def removeClock(self):
        pass
    def removeLru(self):
        pass
    def removeNfu(self):
        pass
