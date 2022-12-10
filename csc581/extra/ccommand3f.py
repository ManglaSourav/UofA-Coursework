# (c) Sandiway Fong, University of Arizona, 2022
from itertools import permutations
from nltk.tree import Tree
import sys
import re
from nltk.corpus import ptb

def dom(x, path):
    if path is None:
        path = list()
    yield x, path
    if not isinstance(x, str):
        path.append(x.label())
        for y in x:
            yield from dom(y, path.copy())

def cc(x):
    if not isinstance(x, str):
        if len(x) > 1:
            for y,z in permutations(x, 2):
                m1 = re.search(yregex, y.label())
                if m1:
                    for w, path in dom(z, None):
                        if isinstance(w, str):
                            m2 = re.search(wregex, w)
                        else:
                            m2 = re.search(wregex, w.label())
                        if m2:
                            print('tree',i+70000,':',y, 'c-commands', w, 'path', path)
            for u in x:
                cc(u)
        else:
            cc(x[0])

if len(sys.argv) == 2:
    with open(sys.argv[1]) as f:
        t  = Tree.fromstring(f.read())
        cc(t)

