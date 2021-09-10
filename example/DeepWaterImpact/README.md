## Compiling

Loading the necessary env (assuming we installed the colza-experiments properly)

```
#!/bin/bash

cd /global/cscratch1/sd/zw241/colza-experiments/cori/vtk

module load cray-python/3.8.5.0
module swap PrgEnv-intel PrgEnv-gnu
module swap gcc/8.3.0 gcc/9.3.0
module load cmake/3.18.2

source sw/spack/share/spack/setup-env.sh
spack env activate colza-env

#margo related var
export MPICH_GNI_NDREG_ENTRIES=1024
# get more mercury info
export HG_NA_LOG_LEVEL=debug

# try to avoid the argobot stack size issue
export ABT_THREAD_STACKSIZE=2097152

cd /global/cscratch1/sd/zw241/build_mona-vtk-matthieu

#cmake ~/cworkspace/src/mona-vtk-matthieu/mona-vtk -DCMAKE_CXX_COMPILER=CC -DCMAKE_C_COMPILER=cc -DENABLE_EXAMPLE=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=./install -DENABLE_DAMARIS=ON -DBOOST_ROOT=`spack location -i boost`
```

If we assume the source code is stored at `~/cworkspace/src/mona-vtk-matthieu/mona-vtk/`

```
cmake ~/cworkspace/src/mona-vtk-matthieu/mona-vtk/ -DCMAKE_CXX_COMPILER=CC -DCMAKE_C_COMPILER=cc  -DENABLE_EXAMPLE=ON -DBUILD_SHARED_LIBS=ON -DENABLE_DAMARIS=ON -DBOOST_ROOT=`spack location -i boost`
```

## Run

Run the test script for mona or mpi testing by `sbatch dw-mona.sbatch` or `sbatch dw-mpi.sbatch`.

We need to update the proper variables such as `SRCDIR`, `BUILDDIR`, `TARFILEPATH` and `TARFILE` before executing the coresponding scripts.

