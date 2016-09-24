JusBrasil
=========

Sample code used as example in talk at JusBrasil.
This repository contains mainly two projects: the **PLSA** and the **HMM**.

Build
-----

To build the code, simply type:

    $ make

And to compile the python module, type:

    $ cd python
    $ python setup.py build_ext --inplace

Running
-------

To run the **PLSA** program, type:

    $ ./plsa -d <DOCINFO> -t <TRAINING_FILE> -i <IGNORE_FILE> -p <PLSA_FILE> -q <NUM_TOPICS> -m <MAX_ITER> -e <TOL> -w <TOP_WORDS> -y <TEST_FILE> -z <TOP_TOPICS>

for instance:

    $ ./plsa -d result.docinfo -t training.txt -i ignore.txt -p result.plsa -q 40 -m 100 -e 0.001 -w 30 -y test.txt -z 5

The files *TRAINING_FILE* and *TEST_FILE* contain a collection of documents
separated by a line containing only the string

    ----------------

Also, each document starts with an identifier number **docnum**.
So these files will look like:

    docnum

    document text
    ------------------------
    docnum

    document text
    ------------------------
    docnum

    document text
    ------------------------
    ....

The *IGNORE_FILE* is just a file containing a list of words to be ignored
from the *TRAINING_FILE*.

To run the **HMM** program type:

    $ ./hmm -d <DOCINFO> -t <TRAINING_FILE> -i <IGNORE_FILE> -h <HMM_FILE> -q <NUM_STATES> -m <MAX_ITER> -e <TOL> -n <NUM_GENERATED_TEXTS>

for instance:

    $ ./hmm -d result.docinfo -t training.txt -i ignore.txt -h result.hmm -q 40 -m 100 -e 0.001 -n 10

and to run the python module, type:

    $ cd python
    $ python test_plsa.py -h
