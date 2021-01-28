from setuptools import setup, Extension

def get_pybind_include():
    """Helper class to determine the pybind11 include path
    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked. """

    import pybind11
    yield pybind11.get_include()

ext_modules = [
    Extension(
        'veripb.optimized.pybindings',
        ['veripb/optimized/pybindings.cpp',
         'veripb/optimized/constraints.cpp',
         'veripb/optimized/parsing.cpp'],
        depends=[
         'veripb/optimized/constraints.hpp',
         'veripb/optimized/BigInt.hpp',
        ],
        include_dirs=
            # Path to pybind11 headers
            get_pybind_include(),
        extra_compile_args=['--std=c++17', '-DPY_BINDINGS'],
        libraries=['gmp', 'gmpxx'],
        language='c++'
    ),
    Extension('veripb.constraints', sources=['veripb/constraints.pyx']),
    # Extension('veripb.rules', sources=['veripb/rules.pyx']),
    # Extension('veripb.autoproving', sources=['veripb/autoproving.pyx']),
]

for e in ext_modules:
    e.cython_directives = {'language_level': "3"} #all are Python-3

setup(
    name='veripb',
    version='0.3a0',
    description='Tool for verifying refutations.',
    url='http://github.com/StephanGocht/veripb',
    author='Stephan Gocht',
    author_email='stephan@gobro.de',
    license='MIT',
    packages=['veripb'],
    setup_requires=[
        # Setuptools 18.0 properly handles Cython extensions.
        'setuptools>=18.0',
        'cython',
        'pybind11'
    ],
    install_requires=[
        'cppimport',
        'cython',
        # 'pyximport', part of cython, no extra pacage exists
        'pybind11'
    ],
    entry_points={
        'console_scripts': [
            'veripb=veripb:run_cmd_main',
        ]
    },
    ext_modules = ext_modules
)
