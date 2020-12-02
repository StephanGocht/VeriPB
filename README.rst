VeriPB - Verifier for pseudo-Boolean proofs
===========================================
.. image:: https://zenodo.org/badge/DOI/10.5281/zenodo.3548581.svg
   :target: https://doi.org/10.5281/zenodo.3548581

VeriPB is a tool for verifying refutations (proofs of unsatisfiability)
and more (such as verifying that a valid solution is found) in python
and c++. A description of the proof file format can be found below.

Currently it only supports linear pseudo-Boolean proofs using cutting planes.

WARNING
=======
The current default setting only uses fixed bitwdith integers and does
not contain code for catching overflows. This means that trying to
verify proofs that contain or result in large numbers leads to
undefined behaviour. If you need to use large coefficients, you can
try the ``--arbitraryPrecision`` option.

ATTENTION
=========
VeriPB is still in early and active development and to be understood
as a rapidly changing proof of concept (for research publications). A
stable version is to be expected towards the end of my PhD in 2022.
If you want to use VeriPB, e.g. because you need it for your
cutting-edge research or to compare it to other tools, I highly
encourage you to get in contact with me.

Table of Contents
=================
.. contents::
   :depth: 2
   :backlinks: none

Installation
============

::

    git clone git@gitlab.com:miao_research/VeriPB.git
    cd ./VeriPB
    pip3 install --user -e ./

Run ``veripb --help`` for help. Note that the first call to veripb
after installation or update will take a while as it is compiling
code.

Update
------

If installed as described above the tool can be updated with

::
    git pull
    pip3 install --user -e ./

Getting Started
===============

A good way to getting started is probably to have a look at the
examples under ``tests/integration_tests/correct`` and to run VeriPB
with the ``--trace`` option, which will output the derived proof.

For Example::

    veripb --trace tests/integration_tests/correct/miniProof_polishnotation_1.opb tests/integration_tests/correct/miniProof_polishnotation_1.proof


Formula Format
==============

The formula is provided in `OPB <http://www.cril.univ-artois.fr/PB12/format.pdf>`_ format. A short overview can be
found
`here <https://gitlab.com/miao_research/roundingsat/-/blob/master/InputFormats.md>`_.

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
-----

::

    pseudo-Boolean proof version 1.1
    * load formula
    f [nProblemConstraints]
    * compute constraint in polish notation
    p [sequence of operations in reverse polish notation]
    * introduce constraint that is verified by reverse unit propagation
    u  [OPB style constraint]
    * delete constraints
    d [constraintId1] [constraintId2] [constraintId3] ...
    * verify contradiction
    c [which]

Introduction
------------

There are multiple rules, which are described in more detail below.
Every rule has to be written on one line and no line may contain more
than one rule. Each rule can create an arbitrary number of
constraints (including none). The verifier keeps a database of
constraints and each constraint is assigned an index, called
ConstraintId, starting from 1 and increasing by one for every added
constraint. Rules can reference other constraints by their
ConstraintId.

In what follows we will use IDmax to refer to the largest used ID
before a rule is executed.

(f)ormula
---------

::


    f [nProblemConstraints]

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
    1 x1 2 x2 >= 1 ;
    1 x3 1 x4  = 1 ;

with the proof file::

    pseudo-Boolean proof version 1.1
    f 3

will be translated to::

    1: 1 x1 2 x2 >= 1 ;
    2: 1 x3 1 x4 >= 1 ;
    3: -1 x3 -1 x4 >= -1 ;


(c)ontradiction
---------------

::

    c [ConstraintId]

Verify that the constraint [ConstraintId] is contradicting, i.e., it
can not be satisfied.

Examples of contradicting constraints::

    >= 1 ;
    >= 3 ;
    3 x1 -2 x2 >= 4 ;


reverse (p)olish notation
-------------------------

::

    p [sequence in reverse polish notation]

Add a new constraint with ConstraintId := IDmax + 1. How to derive the constraint is describe by a 0 terminated sequence of
arithmetic operations over the constraints. These are written down in
reverse polish notation. We will use ``[constraint]``  to indicate
either a ConstraintId or a subsequence in reverse polish notation.
Available operations are:

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

* Literal Axioms::

    [literal]
    x1
    ~x1

Where ``[literal]`` is a variable name or its negation (``~``) and
generates the constraint that the literal is greater equal zero.
For example for ``~x1`` this generates the constraint ~x1 >= 0.

* Weakening::

    [constraint] [variable] w

Where ``[variable]`` is a variable name and may not contain negation.
This step adds literal axioms such that ``[variable]`` disapears from
the constraint, i.e., its coefficient becomes zero.

Conclusion
^^^^^^^^^^

This set of instructions allows to write down any treelike refutation
with a single rule.

For example::

    p 42 3 * 43 + s 2 d

Creates a new constraint by taking 3 times the constraint with index
42, then adds constraint 43, followed by a saturation step and a
division by 2.

reverse (u)nit propagation
--------------------------

::

    u [OPB style constraint]

Use reverse unit propagation to check if the constraint is implied,
i.e., it temporarily adds the negation of the constraint and performs
unit propagation, including all other (non deleted) constraints in
the database. If this unit propagation yields contradiction then we
know that the constraint is implied and the check passes.

If the reverse unit propagation check passes then the constraint is
added with ConstraintId := IDmax + 1. Otherwise, verification fails.

It is also possible to introduce redundant constraints that can be
checked with unit propagation.

::

    u w [literal1] [literal2] ... ; [OPB style constraint]

Adding the constraint is successful if it passes the map redundancy
check via unit propagation or syntactic checks, i.e., if it can be
shown that every assignment satisfying the constraints in the database
:math:`F` but falsifying the to-be-added constraint :math:`C` can be
transformed into an assignment satisfying both by using the
assignment (or witness) :math:`\omega` provided by the list of
literals. More formally it is checked that,

.. math::
    F \land \neg C \models (F \land C)\upharpoonright\omega .

For details, please refer to [GN21]_.


(d)elete constraint
-------------------

::

    d [constraintId1] [constraintId2] [constraintId3] ...

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
-----

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
--------

::

    e [C: ConstraintId] [D: OPB style constraint]

Verify that C is the same constraint as D, i.e. has the same degree
and contains the same terms (order of terms does not matter).

(i)mplies
---------

::

    i [C: ConstraintId] [D: OPB style constraint]

Verify that C syntactically implies D, i.e. it is possible to derive D
from C by adding literal axioms.

(j) implies and add
-------------------

Identical to (i)mplies but also adds the constraint that is implied
to the database with ConstraintId := IDmax + 1.

(#) set level
-------------

::

    # [level]

This rule does mark all following constraints, up to the next
invocation of this rule, with ``[level]``. ``[level]`` is a
non-negative integer. Constraints which are generated before the first
occurrence of this rule are not marked with any level.

(w)ipeout level
---------------

::

    w [level]

Delete all constraints (see deletion command) that are marked with
``[level]`` or a greater number. Constraints that are not marked with
a level can not be removed with this command.

Example
-------

::

    pseudo-Boolean proof version 1.0
    f 10 0              # IDs 1-10 now contain the formula constraints
    p 1 x1 3 * + 42 d 0 # Take the first constraint from the formula,
                          weaken with 3 x_1 >= 0 and then divide by 42


Beyond Refutations
==================

TLDR;
-----

::

    * new solution
    v [literal] [literal] ...
    * new optimal value
    o [literal] [literal] ...

(v) solution
------------

::

    v [literal] [literal] ...
    v x1 ~x2

Given a partial assignment in form of a list of ``[literal]``, i.e.
variable names with ``~`` as prefix to indicate negation, check that:

* after unit propagation we are left with a full assignment, i.e. an
  assignment that assigns all variables that are mentioned in a
  constraint in the formula or the proof

* the full assignment does not violate any constraint

If the check is successful then the clause consisting of the negation
of all literals is added with ConstraintId := IDmax + 1. If the check
is not successful then verification fails.

(o) optimal value
-----------------

::

    o [literal] [literal] ...
    o x1 ~x2

This rule can only be used if the OPB file specifies an objective
function :math:`f(x)`, i.e., it contains a line of the form::

    min: [coefficient] [literal] [coefficient] [literal] ...

Given a partial assignment :math:`\rho` in form of a list of ``[literal]``, i.e.
variable names with ``~`` as prefix to indicate negation, check that:

* every variable that occurs in the objective function is set

* after unit propagation we are left with a full assignment, i.e. an
  assignment that assigns all variables that are mentioned in a
  constraint in the formula or the proof

* the full assignment does not violate any constraint

If the check is successful then the constraint :math:`f(x) \leq
f(\rho) - 1` is added with ConstraintId := IDmax + 1. If the check is
not successful then verification fails.

Debugging and for Development Only
==================================

TLDR;
-----

::

    * add constraint as unchecked assumption
    a [OPB style constraint]

(a) unchecked assumption
------------------------

::

    * add constraint as unchecked assumption
    a [OPB style constraint]

Adds the given constraint without any checks. The constraint gets
ConstraintId := IDmax + 1. Proofs that contain this rule are not
valid, because it allows adding any constraint. For example one could
simply add contradiction directly.

This rule is intended to be used during solver development, when not
all aspects of the solver have implemented proof logging, yet. For
example, imagine that the solver knows by some fancy algorithm that it
is OK to add a constraint C, however proof logging for the derivation
of C is not implemented yet. Using this rule we can simply add C
without providing a derivation and check with VeriPB that all other
derivations that are already implemented are correct.

References
==========

.. _GN21:

[GN21] Certifying Parity Reasoning Efficiently Using Pseudo-Boolean Proofs,
Stephan Gocht, Jakob Nordström, (to apear 2021).
