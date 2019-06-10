refpy Refutations in Python
====

refpy is a tool for verifying refutaitons (proofs of unsatisfiablity)
in python. A description of the proof file format follows.

Currently it only supports pseudo-Boolean refutations.

Installation
============

::

    git clone git@github.com:StephanGocht/refpy.git
    pip3 install -e ./refpy

run ``refpy --help`` for help

Update
------

If installed as described above the tool can be updated with ``git pull``.


Formula Format
==============

The formula is provided in `OPB<http://www.cril.univ-artois.fr/PB10/format.pdf>`_ format. A short overview can be
found
`here<https://github.com/elffersj/roundingsat/blob/proof_logging/InputFormats.md>`_.

Proof Format
============
TLDR;
----

::

    refutation using f l p r c e 0
    f [nProblemConstraints] 0
    l [nVars] 0
    p [sequence of operations in reverse polish notation] 0
    r [antecedent1] [antecedent2] ... 0
    c [which] 0
    e [which] opb [OPB style constraint]

Introduction
----

There are multiple rules, which are described in more detail below.
Each rule can create an arbitrary number of constraints (including
none). The verifyer keeps a database of constraints and each
constraint is assigned an index, called ConstraintId, starting from 1.
Rules can reference other constraints by their ConstraintId.

In what follows we will use IDmax to refer to the largest used ID
before a rule is executed.

(f)ormula
----

::


    f [nProblemConstraints] 0

This rule loads all axioms from the input formula (the path to the
formula will be provided separately when calling the proof checker).

The value of nProblemConstraints is the number of constraints counting
equalities twice. This is because equalities in the input formula are
replaced by two inequalities, where the first inequality is '>=' and
the second '<='. Afterwards, the i-th inequality in the input formula
gets ID := IDmax + i.


For example the opb file::

    * #variable= 3 #constraint= 1
    1x1 2x2 >= 1;
    1x3 1x4  = 1;

with the proof file::

    refutation using f 0
    f 3 0

will be translated to::

    1: 1x1 2x2 >= 1;
    2: 1x3 1x4 >= 1;
    3: -1x3 -1x4 >= -1;



(l)iteral axiom
----

::

    l [nVars] 0

Create literal axioms for i = 1 to i <= nVars:
* 0   <= x_i gets ID := IDmax + 2i - 1
* x_i <= 1 gets ID := IDmax + 2i

Note that variables are required to start from 1.

For example the proof file::

    refutation using f 0
    l 2 0

will be translated to::

    1: 1x1 >= 0
    2: -1x1 >= -1
    3: 1x2 >= 0
    4: -1x2 >= -1

(r)esolution
----

::

    r [antecedent1] [antecedent2] ... 0

Performs multiple (input) resolution steps. Requires antecedents to be
clausal (degree 1).


(c)ontradiction
----

::

    c [ConstraintId] 0

Verify that the constraint [ConstraintId] is contradicting.


(e)quals
----

::

    e [ConstraintId] opb [OPB style constraint]

    e [ConstraintId] cnf [DIMACS style clause]

Verify that constraitn [ConstraintId] is euqal to [OPB style constraint].

reverse (p)olish notation
----

::

    p [sequence in reverse polish notation] 0

The refutation itself is constructed by a 0 terminated sequence of
arithmetic operations over the constraints. These are written down in
reverse polish notation. Available operations are:

* Addition::

    [constraint] [constraint] +

* Scalar Multiplication::

    [constraint] [factor] *

The factor is a strictly positive integer and needs to be the second
operand.

* Boolean Division::

    [constraint] [divisor] d

The divisor is a strictly positive integer and needs to be the second
operand.


* Boolean Saturation::

    [constraint] s

Where [constraint] is either a ConstraintId or a subsequence in
reverse polish notation.

This allows to write down any treelike refutation with a single rule.

For example::

    p 42 3 * 43 + s 2 d 0

Creates a new constraint by taking 3 times the constraint with index
42, then adds constraint 43, followed by a saturation step and a
division by 2.

Example
----

::

    refutation graph using f l p 0
    l 5 0               # IDs 1-10 now contain literal axioms
    f 10 0              # IDs 11-20 now contain the formula constraints
    p 11 1 3 * + 42 d 0 # Take the first constraint from the formula,
                          weaken with 3 x_1 >= 0 and then divide by 42