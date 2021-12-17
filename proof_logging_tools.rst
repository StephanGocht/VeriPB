RoundingSAT
===========

https://gitlab.com/MIAOresearch/roundingsat/

Instances
---------

RoundingSAT can solve any problem in the OPB format. A natural choice
would be the instances from the
`PB16 competition <http://www.cril.univ-artois.fr/PB16/>`_.
(`direct link <http://www.cril.univ-artois.fr/PB16/bench/PB16-used.tar>`_.)


Execution with proof logging
----------------------------

Running the command

::

    ./roundingsat instance.opb --proof-log=out

will generate a file ``out.proof`` containing the proof log.

The Glasgow Subgraph Solver
===========================


https://github.com/ciaranm/glasgow-subgraph-solver

Instances
---------

Uses dimacs graphs as instances, a collection of subgraph instances
can be found at https://perso.liris.cnrs.fr/christine.solnon/SIP.html

Execution with proof logging
----------------------------

For finding subgraphs:

::

    ./glasgow_subgraph_solver pattern_graph target_graph --prove out --no-clique-detection

Will generate files ``out.opb`` and ``out.veripb``


For finding cliques:

::

    ./glasgow_clique_solver graph --prove out

Will generate files ``out.opb`` and ``out.veripb``


For finding common subgraphs:

::

    ./glasgow_common_subgraph_solver graphA graphB --prove out

Will generate files ``out.opb`` and ``out.veripb``


Glasgow Constraint Solver (WIP)
===============================

https://github.com/ciaranm/glasgow-constraint-solver



MiniSAT with XorEngine
======================

https://gitlab.com/MIAOresearch/minisat_with_xorengine

Instances
---------

Any CNF instances, e.g., from the `SAT competition <https://satcompetition.github.io/2020/>`_.

Execution with proof logging
----------------------------

Compile with ``make pbp`` to enable proof logging.

::

    ./bin/minisat_pbp instances.cnf


Will generate file ``proof.txt``.

To verify CNF instances with VeriPB it is neccessary to use the
``--cnf`` option. E.g.,

::

    veripb --cnf instance.cnf proof.txt


BreakID
=======

https://bitbucket.org/krr/breakid/

Instances
---------

Any CNF instances, e.g., from the `SAT competition <https://satcompetition.github.io/2020/>`_.

Execution with proof logging
----------------------------

::

    ./BreakID -f test.cnf -no-bin -no-relaxed -logfile out.pbp

To verify CNF instances with VeriPB it is neccessary to use the
``--cnf`` option. E.g.,

::

    veripb --cnf instance.cnf proof.txt


Kissat Hack
===========

https://gitlab.com/MIAOresearch/kissat_fork

Instances
---------

Any CNF instance

Execution with proof logging
----------------------------

::

    ./kissat instance.cnf out.pbp --no-binary