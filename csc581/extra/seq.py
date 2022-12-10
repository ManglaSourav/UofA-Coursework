seq = [['ADJ', 'ADJ', 'NOUN', 'VERB', 'ADV'],
           ['ADV', 'VERB', 'NOUN', 'ADJ', 'ADJ'],
           ['ADV', 'VERB', 'NOUN', 'NOUN', 'ADJ'],
           ['ADV', 'VERB', 'NOUN', 'VERB', 'ADJ']]

def find_tagseq(seq, tagws):
      ans = []
      n = len(seq)
      for idx in (i for i,tup in enumerate(tagws[:-n+1]) if tup[1]==seq[0]):
          d = dict(tagws[idx:idx+n])
          if list(d.values()) == seq:
              ans.append(list(d.keys()))
      return ans


