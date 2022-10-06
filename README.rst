VeriPB - Verifier for pseudo-Boolean proofs
===========================================
.. image:: https://zenodo.org/badge/DOI/10.5281/zenodo.3548581.svg
   :target: https://doi.org/10.5281/zenodo.3548581

VeriPB is a tool for verifying refutations (proofs of
unsatisfiability) and more (such as verifying that a valid solution is
found) written in python and c++. A quick overview of the proof file
format can be found below.

Currently its focus is on linear pseudo-Boolean proofs utilizing
cutting planes reasoning. VeriPB has already been used for various
applications including proof logging of

* subgraph isomorphism [GMN2020]_,
* clique and maximum common (connected) subgraph [GMMNPT2020]_,
* constraint programming with all different constraints [EGMN20]_,
* parity reasoning in the context of CDCL SAT solvers [GN21]_ and
* dominance and symmetry breaking [BGMN22]_.


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

The following tools and libraries are required (with minimal suggested versions):

* Python 3.6.9 with pip and setuptools installed
* g++ 7.5.0
* libgmp

These can be installed in ubuntu / debian via

::

    apt-get update && apt-get install \
            python3 \
            python3-pip \
            python3-dev \
            g++ \
            libgmp-dev \
        && pip3 install \
            setuptools


When these requirenments are met, VeriPB can be installed via

::

    git clone git@gitlab.com:MIAOresearch/VeriPB.git
    cd ./VeriPB
    pip3 install --user ./

Run ``veripb --help`` for help.

Update
------

If installed as described above the tool can be updated form the VeriPB directory with

::

    git pull
    pip3 install --user ./

Getting Started
===============

A good way to getting started is probably to have a look at the
examples under ``tests/integration_tests/correct`` and to run VeriPB
with the ``--trace --useColor`` option, which will output the derived proof.

For Example::

    veripb --trace --useColor tests/integration_tests/correct/miniProof_polishnotation_1.opb tests/integration_tests/correct/miniProof_polishnotation_1.pbp


Formula Format
==============

The formula is provided in `OPB <http://www.cril.univ-artois.fr/PB12/format.pdf>`_ format. A short overview can be
found
`here <https://gitlab.com/MIAOresearch/roundingsat/-/blob/master/InputFormats.md>`_.

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
    pol [sequence of operations in reverse polish notation]
    * introduce constraint that is verified by reverse unit propagation
    rup  [OPB style constraint]
    * delete constraints
    del [constraintId1] [constraintId2] [constraintId3] ...
    * verify contradiction
    c [which]
    * add constraint by redundancy based strengthening
    red [OPB style constraint] ; [substitution]
    * add constraint by dominance based strengthening
    dom [OPB style constraint] ; [substitution]

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


reverse (pol)ish notation
-------------------------

::

    pol [sequence in reverse polish notation]

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

    pol 42 3 * 43 + s 2 d

Creates a new constraint by taking 3 times the constraint with index
42, then adds constraint 43, followed by a saturation step and a
division by 2.

reverse unit propagation (RUP)
--------------------------

::

    rup [OPB style constraint]

Use reverse unit propagation to check if the constraint is implied,
i.e., it temporarily adds the negation of the constraint and performs
unit propagation, including all other (non deleted) constraints in
the database. If this unit propagation yields contradiction then we
know that the constraint is implied and the check passes.

If the reverse unit propagation check passes then the constraint is
added with ConstraintId := IDmax + 1. Otherwise, verification fails.


(del)ete constraint
-------------------

::

    del id [constraintId1] [constraintId2] [constraintId3] ...
    del spec [OPB style constraint]
    del range [constraintIdStart] [constraintIdEnd]

Delete constraints with given constrain ids, spacification or in the
range from start to end, including start but not end. Note that
constraints will be deleted completely including propagations caused.

If an order is loaded and a constarint marked as core is deleted, then
additional checks might be required.

Strengthening Rules
===================

Substitution
------------

A substitution ``[substitution]`` is a space seperated sequence of
multiple mappings from a variable to a constant or a literal.

::

    [variable] -> 0
    [variable] -> 1
    [variable] -> [literal]

Using ``->`` is optional and can improve readability.

For example::
    x1 -> 0 x2 -> ~x3
    x1 0 x2 ~x3


Redundancy Based Strengthening
------------------------------


::

    red [OPB style constraint] ; [substitution]


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

If the redundancy rule is used in the context of optimization and / or
dominance breaking, additional conditions are checked. For details,
please refer to [BGMN22]_.

Subproofs
---------

For both strengthening rules it is possible to provide an explicit
subproof. A suproof starts by ending the strengthening step with ``;
begin`` and is concluded by ``end``. Within a subproof it is possible
to specify proof goals using ``proofgoal [goalId]``, which are in turn
terminated by ``end``. Each proofgoal needs to derive contradiction
using the provided constraints.

Example ::

    red 1 x1 >= 1 ; x1 -> 1 ; begin
        proofgoal #1
            pol -1 -2 +
            c -1
        end

        proofgoal 1
            rup >= 1 ;
            c -1
        end
    end

The ``[goalId]`` are as follows: If a goal originates from a
constraint in the database the ``[goalId]`` is identical to the
constraintId of the constraint in the database. Otherwise the goalId
starts with a ``#`` folowed by a number which is increased for each
goal in the following order (if applicable): the constraint to be
derived (only redundancy), one goal per constraint in the order, one
goal for the negated order (only dominance), objective condition (only
for optimization problems). Tip: Use ``--trace`` option to display
required goals.

Dominance Based Strengthening
-----------------------------

For details, please refer to [BGMN22]_. For syntax have a look at the
example under ``tests/integration_tests/correct/dominance/example.pbp`` .

Template: ::

    pre_order simple
        * specify variables
        vars
            left u1
            right v1
        end

        * define the order
        def
            -1 u1 1 v1 >= 0 ;
        end

        * proof goal: transitivity
        transitivity
            vars
                fresh_right w1
            end
            proof
                proofgoal #1
                    p 1 2 + 3 +
                    c -1
                qed
            qed
        qed
    end

    load_order simple x1
    dom 1 ~x1 >= 1 ; x1 0


Moving constraints to core
--------------------------

::

    core id [constraintId1] [constraintId2] ...
    core spec [opb style constraint]
    core range [constraintIdStart] [constraintIdEnd]


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

* after unit propagation we are left with a full assignment to the
  current database, i.e. an assignment that assigns all variables that
  are mentioned in a constraint in the formula or the proof

* the full assignment does not violate any constraint in the current
  database

If the check is successful then the clause consisting of the negation
of all literals is added with ConstraintId := IDmax + 1. If the check
is not successful then verification fails.

(ov) original solution
------------

::

    ov [literal] [literal] ...
    ov x1 ~x2

Given an assignment in form of a list of ``[literal]``, i.e.
variable names with ``~`` as prefix to indicate negation, check that:

* each constraint in the original formula is satisfied

If the check is not successful then verification fails.

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

Acknowledgements
================

VeriPB was developed by Stephan Gocht. The underlying proof system was
designed jointly by Bart Bogaerts, Stephan Gocht, Ciaran McCreesh, and
Jakob Nordström, while investigating and implementing proof logging
for different applications. We are also grateful to Jo Devriendt and
Jan Elffers for many valuable discussions that have helped to improve
the performance of VeriPB.

This work was done in part while the author Stephan Gocht

* was supported by the Swedish Research Council grant 2016-00782
* was participating in a program at the Simons Institute for the Theory of Computing. 


References
==========

.. _GNMO22:

[GMN22] Stephan Gocht, Jakob Nordström Ruben Martins and Andy Oertel.
Certified CNF Translations for Pseudo-Boolean Solving. In Proceedings
of the 25nd International Conference on Theory and Applications of
Satisfiability Testing (SAT '22), 2022 (to appear).


.. _BGMN22:

[BGMN22] Certified Symmetry and Dominance Breaking for Combinatorial
Optimisation, Bart Bogaerts, Stephan Gocht, Ciaran McCreesh, Jakob
Nordström, Proceedings of the AAAI Conference on Artificial
Intelligence, 2022 (to appear).

.. _GN21:

[GN21] Certifying Parity Reasoning Efficiently Using Pseudo-Boolean
Proofs, Stephan Gocht, Jakob Nordström, Proceedings of the AAAI
Conference on Artificial Intelligence, 2021, 35, 3768-3777.

.. _GMN21:

[GMN21] Stephan Gocht, Ciaran McCreesh and Jakob Nordström. VeriPB:
The Easy Way to Make Your Combinatorial Search Algorithm Trustworthy.
From Constraint Programming to Trustworthy AI, workshop at the 26th
International Conference on Principles and Practice of Constraint
Programming (CP '20), September 2020. `[PDF]
<http://www.jakobnordstrom.se/docs/publications/VeriPB_CPTAI2020.pdf>`_
`[VIDEO] <https://www.youtube.com/watch?v=SQ1-lF9clHQ>`_

.. _GMMNPT2020:

[GMMNPT2020] Stephan Gocht, Ross McBride, Ciaran McCreesh, Jakob Nordström, Patrick
Prosser, and James Trimble. Certifying Solvers for Clique and Maximum
Common (Connected) Subgraph Problems. In Proceedings of the 26th
International Conference on Principles and Practice of Constraint
Programming (CP '20), Lecture Notes in Computer Science, volume 12333,
pages 338-357, September 2020.

.. _GMN2020:

[GMN2020] Stephan Gocht, Ciaran McCreesh, and Jakob Nordström. Subgraph
Isomorphism Meets Cutting Planes: Solving with Certified Solutions. In
Proceedings of the 29th International Joint Conference on Artificial
Intelligence (IJCAI '20), pages 1134-1140, July 2020.

.. _EGMN20:

[EGMN20] Jan Elffers, Stephan Gocht, Ciaran McCreesh, and Jakob Nordström.
Justifying All Differences Using Pseudo-Boolean Reasoning. In
Proceedings of the 34th AAAI Conference on Artificial Intelligence
(AAAI '20), pages 1486-1494, February 2020.

