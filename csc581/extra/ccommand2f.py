# (c) Sandiway Fong, University of Arizona, 2022
from itertools import permutations
from nltk.tree import Tree
import sys

def dom(x):
    yield x
    if not isinstance(x, str):
        for y in x:
            yield from dom(y)
            
def cc(x):
    if not isinstance(x, str):
        if len(x) > 1:
            for y,z in permutations(x, 2):
                for w in dom(z):
                    print(y, 'c-commands', w)
            for u in x:
                cc(u)
        else:
            cc(x[0])

if len(sys.argv) == 2:
    with open(sys.argv[1]) as f:
        t  = Tree.fromstring(f.read())
        cc(t)
