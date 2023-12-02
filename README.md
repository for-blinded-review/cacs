# CACS

This repository is intended for peer review of the paper _stackful coroutine made fast_. 

It includes source code of:
* an early version of stackful coroutine library for ease of use in the experiments: **/common/photonlib**
* in-stack generator: **/common/photonlib/gen.h**
* scheduler for boost: **/common/fibsched**
* scheduler for C++20 stackless coroutine: **/common/co20executor**
* benchmarks: **/*-test**

Our modifications to Clang 15.03 are placed in another repo at: https://github.com/for-blinded-review/llvm-project

The relavent commits are:
* [[PATCH 1/2] Add preserve_none calling convension](https://github.com/for-blinded-review/llvm-project/commit/4aed4ec8b32a3abc24f805d2a68e5d3903e19f45)
* [[PATCH 2/2] add x86-disable-align-stack-via-push option to disable us…](https://github.com/for-blinded-review/llvm-project/commit/2e84059271faacccbb9a7d25347a7d56681cf294)


We'll change the name and repo url into real ones, after passing the blided review process.


Thanks!
