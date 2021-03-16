#!/bin/bash

#use the system default python env, which is 3.6
module load spack
module load cmake/3.18.2
# for compiling vtk on cori
export CRAYPE_LINK_TYPE=dynamic

# let cc and CC to be the gnu compier
module swap PrgEnv-intel PrgEnv-gnu

# make sure the package.yaml has the same compiler version
module swap gcc/8.3.0 gcc/9.3.0
spack load -r mochi-colza@main%gcc@9.3.0

#important! make sure the cc can find the correct linker, the spack may load the wrong linker
#this is only necessary for compiling the paraview
#spack load -r mesa/qozjngg
#PATH="/global/common/cori/software/altd/2.0/bin:$PATH"

cd $SCRATCH/build_monavtk

export MPICH_GNI_NDREG_ENTRIES=1024
# get more mercury info
export HG_NA_LOG_LEVEL=debug

# try to avoid the argobot stack size issue
# export ABT_THREAD_STACKSIZE=2097152