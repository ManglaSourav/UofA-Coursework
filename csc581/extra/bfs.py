# Example of breadth-first search of WordNet relations in Python,
# based on my single-threaded Perl code: bfs4.perl
# Sandiway Fong, University of Arizona (2021)
#
# usage: python3 bfs.py word.pos.sense word.pos.sense [-m max]
# synopsis: finds the shortest semantic links between two word senses
# looks for all paths of minimum length (up to max number of nodes visited)
# prints the list of relations connecting the two word senses
# nltk format:
# Synset: word.pos.nn, nn = sense number, e.g. minibike.n.01
# Lemma:  word.pos.nn.lemma, e.g. minibike.n.01.motorbike

import math
from collections import deque
import itertools
import networkx as nx
from networkx.drawing.nx_agraph import graphviz_layout
import matplotlib.pyplot as plt
import argparse

p = argparse.ArgumentParser(
    description="breadth-first search of nltk WordNet relations with 2D and 3D graphing.",
    epilog="""Word sense: word.pos.sense, e.g. minibike.n.01 or minibike.n.1; pos = [asrnv]
Run in interactive mode (-i): use lookup(sense1, sense2) then search() to add to the graph.
Plot using draw_graph().""")
p.add_argument('start', help="word sense pair start")
p.add_argument('end', help="word sense pair end")
p.add_argument('others', nargs='*', help="optional additional pairs to search")
p.add_argument('-nd', '--nodraw', help="don't draw anything, use with -i flag to drop into the interpreter.", action='store_true')
p.add_argument('-m','--max', type=int, default=100000,
                   help="maximum number of nodes to be explored (default: 100000)")
args = p.parse_args()

start = ''
end = ''
sname = ''
ename = ''
sname_list = []
ename_list = []

import nltk
from nltk.corpus import wordnet as wn
# wn.synset('motorbike.n.1') => Synset('minibike.n.01')
# Lemma('minibike.n.01.motorbike')

def lookup(sense1, sense2):               # look up word senses in WordNet
    global start
    try:
        start = wn.synset(sense1)
    except Exception:
        print("Can't find {}!".format(sense1))
        return False
    global end
    try:
        end = wn.synset(sense2)
    except Exception:
        print("Can't find {}!".format(sense2))
        return False
    global sname
    sname = start.name()
    global ename
    ename = end.name()
    print('start:', sense1, 'is in synset', sname, '\nend: ',
           sense2, 'is in synset', ename)
    global sname_list
    sname_list.append(sname)
    global ename_list
    ename_list.append(ename)
    return True

def lookup2(ss1, ss2):                    # already looked up synsets ss1,2
    global start
    start = ss1
    global end
    end = ss2
    global sname
    sname = start.name()
    global ename
    ename = end.name()
    print('start:', sname, '\nend: ', ename)
    global sname_list
    sname_list.append(sname)
    global ename_list
    ename_list.append(ename)
    return True

def isLemma(x):
    return isinstance(x, nltk.corpus.reader.wordnet.Lemma)

def isSynset(x):
    return isinstance(x, nltk.corpus.reader.wordnet.Synset)

# abbreviates too long derivationally_related_forms to deriv_related
def pname(x):                             # print name 
    if isLemma(x):
        return x.synset().name()
#        return r'{}.{}'.format(x.synset().name(),x.name())
    elif isinstance(x, str):
        if x == 'derivationally_related_forms':
            return 'deriv_related'
        else:
            return x
    else:
        return x.name()
    
def done(x, y):
    if x == y:
        return True
    else:
        return False


G = nx.DiGraph()
eld = {}                                  # edge label dict

def reportpath(p, dist, n):               # print solution path
    print(r'Found at distance {} ({} nodes expanded)'.format(dist, n))
    global found
    found = True
    for item in p:
        print(pname(item), end=' ')
    print()
    while len(p) > 1:                     # update graph G
        n1 = pname(p[-1])
        n2 = pname(p[-3])
        G.add_edge(n1, n2)
        eld[(n1, n2)] = pname(p[-2])
        p.pop()
        p.pop()

def draw_graph():
    graph = G
    pos=graphviz_layout(graph)            # node:[x,y]
    node_colors = []
    for node in graph:
        if pname(node) in sname_list or pname(node) in ename_list:
            node_colors.append('red')
        else:
            node_colors.append('lightblue')

    nx.draw(graph, pos, node_color=node_colors, with_labels = True)
    nx.draw_networkx_edge_labels(graph, pos, edge_labels = eld)
    plt.show()

relations = ['hypernyms', 'hyponyms' , 'instance_hypernyms', 'instance_hyponyms', 'member_meronyms', 'substance_meronyms', 'part_meronyms', 'member_holonyms', 'substance_holonyms', 'part_holonyms', 'entailments', 'causes']

# lemma-only relations
lrelations = ['antonyms','derivationally_related_forms', 'pertainyms', 'also_sees']

def search():                                # search: start to end
    found = False
    distance = 0
    n = 0
    seen = set()                                   # set

    if start == end:
        found = True
        print(r'Found at distance {} ({} nodes expanded)'.format(distance, n))
    else:
        queue = deque([deque([start]), 'm@rk'])
        while n < args.max:
            node = queue.pop()

            if len(queue) == 0:
                print("Search space complete")
                break
            elif isinstance(node, str) and node == 'm@rk':
                if found:
                    print("Search space complete")
                    break
                else:
                    distance += 1
                    queue.appendleft('m@rk')
                    node = queue.pop()
                    
            for rel in relations:         # synset relations first
                for newnode in getattr(node[0], rel)():
                    n += 1
                    if done(newnode, end):
                        found = True
                        node.appendleft(rel)
                        node.appendleft(newnode)
                        reportpath(node, distance, n)
                    elif newnode not in seen:
                        cnode = node.copy()
                        cnode.appendleft(rel)
                        cnode.appendleft(newnode)
                        queue.appendleft(cnode)

            for lemma in node[0].lemmas():   # lemma relations next
                for lrel in lrelations:
                    for newlnode in getattr(lemma, lrel)():
                        n += 1
                        newnode = newlnode.synset()   # convert up to synset
                        if done(newnode, end):
                            found = True
                            node.appendleft(lrel)
                            node.appendleft(newnode)
                            reportpath(node, distance, n)
                        elif newnode not in seen:
                            cnode = node.copy()
                            cnode.appendleft(lrel)
                            cnode.appendleft(newnode)
                            queue.appendleft(cnode)

    if not found:
        print("Not found (distance", distance, "and", n,"nodes expanded)")
    else:
        print(G)

found = False
pairs =  len(args.others)
if (pairs % 2) == 1:
    print('Error: must supply pairs of word senses!')
else:
    if lookup(args.start, args.end):
        search()

    if pairs > 0:
        for start2, end2 in zip(args.others[0::2],args.others[1::2]):
            if lookup(start2, end2):
                search()
    
if found:
    if not args.nodraw:
        draw_graph()
