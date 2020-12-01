from setuptools import setup, Extension

def get_pybind_include():
    """Helper class to determine the pybind11 include path
    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked. """

    import pybind11
    return pybind11.get_include()

ext_modules = [
    Extension(
        'veripb.optimized.pybindings',
        # Sort input source files to ensure bit-for-bit reproducible builds
        # (https://github.com/pybind/python_example/pull/53)
        ['veripb/optimized/pybindings.cpp',
         'veripb/optimized/constraints.cpp',
         'veripb/optimized/parsing.cpp'],
        depends=[
         'veripb/optimized/constraints.h',
        ],
        include_dirs=[
            # Path to pybind11 headers
            get_pybind_include(),
        ],
        extra_compile_args=['--std=c++17', '-DPY_BINDINGS'],
        libraries=['gmp', 'gmpxx'],
        language='c++'
    ),
    Extension('veripb.constraints', sources=['veripb/constraints.pyx']),
    Extension('veripb.rules', sources=['veripb/rules.pyx']),
]

setup(
    name='veripb',
    version='0.2-alpha',
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
