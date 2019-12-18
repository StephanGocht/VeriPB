from setuptools import setup

setup(
    name='veripb',
    version='0.1',
    description='Tool for verifying refutations.',
    url='http://github.com/StephanGocht/veripb',
    author='Stephan Gocht',
    author_email='stephan@gobro.de',
    license='MIT',
    packages=['veripb'],
    install_requires=[
        "recordclass",
        "cppimport",
        "cython"
    ],
    entry_points={
        'console_scripts': [
            'veripb=veripb:run_cmd_main',
        ]
    },
)
