refpy Refutations in Python
====

refpy is a tool for verifying refutaitons (proofs of unsatisfiablity)
in python. A description of the proof file format follows.

Currently it only supports pseudo-Boolean refutations.

TLDR;
=====

refutation using r d s f l 0
f [nProblemConstraints] 0
l [nVars] 0
r [antecedent1] [antecedent2] ... 0
d [divisor] [factor1] [antecedent1] [factor2] [antecedent2] ... 0
s [factor1] [antecedent1] [factor2] [antecedent2] ... 0
c [which] 0
e [which] opb [OPB style constraint]

Motivation
==========

I would like the format to be as general as possible, as I am planning
to also use it for other research that needs to analyse proof
structure. An additional requirement is that it should be easy to
convert into a binary format, in case this seems beneficial
performance wise at some point. (Essentially, every line will just be
a sequence of non-zero numbers so this can be achieved by defining a
word width, e.g., 32bit and then dumping the number stream without
spaces.)

Each file line starts with an integer (char) that indicates which rule
is used and ends with a 0. One file line can potentially create
multiple proof lines. Each proof line is associated with an ID
(integer greater zero starting from 1), for antecedents only smaller
IDs might be used, so that the resulting proof will trivially be a
DAG.

The first line is the header and contains a list of used rules (just
to have a simple way to check if the given proof is compatible with
the proof checker).

For starters I would suggest the following proof rules, in which IDmax
is the largest used ID before a file line is executed.


(f)ormula
=========

    f [nProblemConstraints] 0

Macro to load all axioms from the input formula (path to the formula
will be provided separately when calling the proof checker).

Equalities in the input formula are replaced by two inequalities.
Afterwards, the i-th inequality in the input formula gets
ID := IDmax + i.


(l)iteral axiom
===============

    l [nVars] 0

Create literal axioms (1i) 0 <= x_i, (2i) x_i <= 1 for i = 1 to i <= nVars
(1i) gets ID := IDmax + 2i - 1
(2i) gets ID := IDmax + 2i
(Note that variables are required to start from 1)

(r)esolution
============

    r [antecedent1] [antecedent2] ... 0

Performs multiple (input) resolution steps. Requires antecedents to be
clausal (degree 1).


(d)ivision
==========

    d [divisor] [factor1] [antecedent1] [factor2] [antecedent2] ... 0

Compute round_up(1/d * ([factor1]*[antecedent1] + [factor2]*[antecedent2] + ...))


(s)aturation
============

    s [factor1] [antecedent1] [factor2] [antecedent2] ... 0

Compute saturate([factor1]*[antecedent1] + [factor2]*[antecedent2] + ...)


(c)ontradiction
===============

    c [which] 0

Verify that the constraint [which] is contradicting


(e)qual
======

    e [which] opb [OPB style constraint]

    e [which] cnf [DIMACS style clause]

Verify that constraitn [which] is euqal to [OPB style constraint]

Example
=======

refutation graph using d f l 0
l 5 0          # IDs 1-10 now contain literal axioms
f 10 0         # IDs 11-20 now contain the formula constraints
d 42 1 11 3 1  # Take the first constraint from the formula,
                 weaken with 3 x_1 >= 0 and then divide by 42