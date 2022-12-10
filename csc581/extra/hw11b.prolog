%% syntax rules
parse(Tree) --> s(Tree,[],_List).
parse(Tree) --> sbarq(Tree,[],_List).

sbarq(sbarq(WHNP,SQ),List,Listp) --> whnp(WHNP), sq(SQ, [WHNP|List],Listp).
s(s(NP,VP),List,Listp) --> np(NP), vp(VP,[NP|List],Listp).
s(s(NP,VP),[NP|List],Listp) --> vp_to(VP,List,Listp).
sq(sq(WHNP,VP),[WHNP|List],Listp) --> vp(VP,[WHNP|List],Listp).
sq(sq(DO,NP,VP),List,Listp) --> vbd(DO),np(NP), vp(VP,[NP|List],Listp).

vp_to(vp(TO,VP),List,Listp) --> to(TO), vp_bare(VP,List,Listp).
vp(vp(VBD,WHNP),[WHNP|List],List) --> vb(VBD).
vp(vp(VBD,NP),List,List) --> vbd(VBD), np(NP).
vp(vp(VBD,S),List,Listp) --> vbd(VBD), s(S,List,Listp).
vp(vp(VB,S),List,Listp) --> vb(VB), s(S,List,Listp).
vp_bare(vp(VB,NP),List,List) --> vb(VB), np(NP).
vp_bare(vp(VB,NP),[NP|List],List) --> vb(VB).
np(np(DT,NN)) --> dt(DT), nn(NN).
np(np(NNP)) --> nnp(NNP).
np(np(WP)) --> wp(WP).
whnp(whnp(WP)) --> wp(WP).
%% Lexicon
nnp(nnp(john)) --> [john].
nnp(nnp(mary)) --> [mary].
vbd(vbd(win+ed)) --> [won].
vbd(vbd(try+ed)) --> [tried].
vbd(vbd(do+ed)) --> [did].
vb(vb(do)) --> [do].
vb(vb(win)) --> [win].
vb(vb(try)) --> [try].
nn(nn(race)) --> [race].
dt(dt(the)) --> [the].
dt(dt(a)) --> [a].
wp(wp(who)) --> [who].
wp(wp(what)) --> [what].
to(to(to)) --> [to].
