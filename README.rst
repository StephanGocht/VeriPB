refpy Refutations in Python
====

refpy is a tool for verifying refutations (proofs of unsatisfiability)
and more (such as verifying that all solutions were found) in python
and c++. A description of the proof file format can be found below.

Currently it only supports pseudo-Boolean proofs.

WARNING
=======
The current version only uses fixed bitwdith integers and does not
contain code for catching overflows. This means that trying to verify
proofs that contain or result in large numbers lead to undefined
behaviour.

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

The formula is provided in `OPB <http://www.cril.univ-artois.fr/PB12/format.pdf>`_ format. A short overview can be
found
`here <https://github.com/elffersj/roundingsat/blob/proof_logging/InputFormats.md>`_.

The verifier also supports an extension to OPB, which allows to use
arbitrary variable names instead of x1, x2, ... Variable names must
follow the following properties:

* start with a letter in ``A-Z, a-z``
* are at least two characters long
* may not contain space

The following characters are guaranteed to be supported: ``a-z, A-Z,
0-9, []{}_^``. Support of further characters is implementation
specific and produces an error if unsupported characters are used.

Basic Proof Format
==================
TLDR;
----

::

    pseudo Boolean proof version 1.0
    * load formula
    f [nProblemConstraints] 0
    * compute constraint in polish notation
    p [sequence of operations in reverse polish notation] 0
    * introduce constraint that is verified by reverse unit propagation
    u  [OPB style constraint]
    * delete constraints
    d [constraintId1] [constraintId2] [constraintId3] ... 0
    * verify contradiction
    c [which] 0

Introduction
----

There are multiple rules, which are described in more detail below.
Each rule can create an arbitrary number of constraints (including
none). The verifier keeps a database of constraints and each
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

If the constraint count does not match or is missing then the
behaviour is implementation specific and verification either fails or
the correct value is used (optionally a warning is emitted).


For example the opb file::

    * #variable= 3 #constraint= 1
    1x1 2x2 >= 1;
    1x3 1x4  = 1;

with the proof file::

    pseudo Boolean proof version 1.0
    f 3 0

will be translated to::

    1: 1x1 2x2 >= 1;
    2: 1x3 1x4 >= 1;
    3: -1x3 -1x4 >= -1;


(c)ontradiction
----

::

    c [ConstraintId] 0

Verify that the constraint [ConstraintId] is contradiction, i.e. 0 >=
1 (this is the same as the empty clause).


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

* Literal Axioms::

    [literal]
    x1
    ~x1

Where ``[literal]`` is a variable name or its negation (``~``) and
generates the constraint that the literal is greater equal zero.
For example for ``~x1`` this generates the constraint ~x1 >= 0.


Conclusion
^^^^^^^^^^

This set of instructions allows to write down any treelike refutation
with a single rule.

For example::

    p 42 3 * 43 + s 2 d 0

Creates a new constraint by taking 3 times the constraint with index
42, then adds constraint 43, followed by a saturation step and a
division by 2.

reverse (u)nit propagation
--------------------------

::

    u [OPB style constraint]

Use reverse unit propagation to check if the constraint is implied,
i.e. it assumes the negation of the constraint and all other (non
deleted) constraints in the database and passes if this yields
contradiction by unit propagation.

If the constraint is implied it is added to the database. Otherwise,
verification fails.

(d)elete constraint
-------------------

::

    d [constraintId1] [constraintId2] [constraintId3] ... 0

Delete constraints with given constrain ids. This verifier currently
implements weak propagating semantic for deletion (see below) but will
change to strong semantic in the foreseeable future, possibly keeping
weak propagating semantic via a parameter settings.

Weak semantic
^^^^^^^^^^^^^

The constraints should no longer be used after deletion. It is
implementation specific if verification fails if they are accessed
after deletion. Especially, the verifier is not required to delete
constraints. The goal of the weak semantic is purely for performance
benefits during verification.

Weak propagating semantic
^^^^^^^^^^^^^^^^^^^^^^^^^

Same as weak semantic, but guarantees to keep unit propagations that
were caused by deleted constraints.

Strong semantic
^^^^^^^^^^^^^^^

Constraints are guaranteed to be deleted.


Convenience Rules and Rules for Sanity Checks
=============================================

TLDR;
----

::

    * check equality
    e [ConstraintId] [OPB style constraint]
    * check implication
    i [ConstraintId] [OPB style constraint]
    * add constraint if implied
    j [ConstraintId] [OPB style constraint]
    * set level (for easier deletion)
    # [level]
    * wipe out level (for easier deletion)
    w [level]


(e)quals
----

::

    e [C: ConstraintId] [D: OPB style constraint]

Verify that C is the same constraint as D, i.e. has the same degree
and contains the same terms (order of terms does not matter).

A constraint is equal to a [DIMACS style clause] if it contains the
same variables, all with coefficient 1, and has degree 1.

(i)mplies
----

::

    i [C: ConstraintId] [D: OPB style constraint]

Verify that C syntactically implies D, i.e. it is possible to derive D
from C by adding literal axioms.

(j) implies and add
---

Identical to (i)mplies but also adds the constraint that is implied to
the database.

(#) set level
-------------

::

    # [level]

This rule does mark all following constraints, up to the next
invocation of this rule, with ``[level]``. ``[level]`` is a positive
integer (greater equal zero). Constraints which are generated before
the first occurrence of this rule are not marked with any level.

(w)ipeout level
---------------

::

    w [level]

Delete all constraints (see deletion command) that are marked with
``[level]`` or a greater number.

Example
-------

::

    pseudo Boolean proof version 1.0
    f 10 0              # IDs 1-10 now contain the formula constraints
    p 1 x1 3 * + 42 d 0 # Take the first constraint from the formula,
                          weaken with 3 x_1 >= 0 and then divide by 42


Beyond Refutations
==================

TLDR;
----

::

    v [literal] [literal] ...

(v) solution
------------

::

    v [literal] [literal] ...
    v x1 ~x2

Given a partial assignment in form of a list of ``[literal]``, i.e.
variable names with ``~`` as prefix to indicate negation, check that
after unit propagation we are left with a full assignment that does
not violate any constraint. If the check is successful then the clause
consisting of the negation of all literals is added. If the check is
not successful then verification fails.