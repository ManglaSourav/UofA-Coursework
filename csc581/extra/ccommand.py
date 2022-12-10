# (c) Sandiway Fong, University of Arizona, 2022
from itertools import permutations
from nltk.tree import Tree
t1 = Tree.fromstring("(S (NP I) (VP (V saw) (NP him)))")
t2 = Tree.fromstring("(M A (B (C E) (D F G)))")

def dom(x):
    yield x
    if not isinstance(x, str):
        for y in x:
            yield from dom(y)
            
def cc(x):
    if len(x) > 1:
        for y,z in permutations(x, 2):
            for w in dom(z):
                print(y, 'c-commands', w)
        for u in x:
            cc(u)
