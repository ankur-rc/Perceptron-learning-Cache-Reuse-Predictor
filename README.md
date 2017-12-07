
This is a C++ based implementation of two cache reuse predictors:

1. [Perceptron-learning based Reuse Prediction](http://hpca23.cse.tamu.edu/pdfs/micro2016-perceptron.pdf)  

2. [Sampling-dead Block Prediction](http://dl.acm.org/citation.cfm?id=1934977)

**Master** branch contains the Perceptron learning predictor and branch **sdbp** contains the SDBP code. SDBP code is still WIP.

The traces to run the program can be found at:
http://faculty.cse.tamu.edu/djimenez/614/traces.tar
PS: This is not an enterprise server. Please be judicious while downloading. Also, the files can be taken down anytime.

To run a single trace, run:

    ./run_single.sh <location-to-trace-file>

To run all traces, run:

    ./run_traces.sh <location-to-traces-directory>

To generate a bar-graph comparing the geometric mean speed-up w.r.t LRU, run:

    python calc_gmean.py

Similarly, run the following for Arithmetic Mean of MPKI values per trace:

    python calc_amean.py


The files to check out are: replacement_state.*
