# Sandiway Fong (c) University of Arizona 2019
# simple function to plot Zipf's Law
# assumes matplotlib
from collections import Counter
from math import log, log10
import matplotlib.pyplot as plt

def plot(tokens, text):
    size = len(tokens)
    c = Counter()
    for token in tokens:
        c[token] += 1
    mc = c.most_common()
    ranks = [x for x in range(1,len(mc)+1)]
    freq = [item[1]/size for item in mc]
    plt.plot(ranks,freq, label=text)

def fig():
    plt.figure(1)
    plt.xscale('log')
    plt.xlabel('log(rank)')
    plt.yscale('log')
    plt.ylabel('log(freq)')
    plt.grid(True)
    plt.title('Log-log plot for freq vs rank')
