from setuptools import setup

setup(
    name='refpy',
    version='0.1',
    description='Tool for verifying refutations.',
    url='http://github.com/StephanGocht/refpy',
    author='Stephan Gocht',
    author_email='stephan@gobro.de',
    license='MIT',
    packages=['refpy'],
    install_requires=[
        "recordclass",
        "parsy",
        "cppimport"
    ],
    entry_points={
        'console_scripts': [
            'refpy=refpy:run_cmd_main',
        ]
    },
)
