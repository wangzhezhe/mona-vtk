#!/bin/bash
source ~/.color

source ~/cworkspace/src/spack/share/spack/setup-env.sh
module load cmake/3.20.5
# for compiling vtk on cori
export CRAYPE_LINK_TYPE=dynamic

# load mochi package
#spack load -r mochi-thallium
#spack load -r mochi-ssg
#spack load -r mochi-abt-io
#spack load mochi-thallium
#spack load mochi-ssg

# let cc and CC to be the gnu compier
module swap PrgEnv-intel PrgEnv-gnu
# uset the gcc7.3.0 since the python3 uses this
# otherwise, there is byte code error
# use the gcc8.2.0, since osmesa use this version
# it looks the osmesa is installed in the system lib, we do not use the spack version
# keep sure allpackages are compiled by same gcc
# make sure the package.yaml has the same compiler version
module swap gcc/8.3.0 gcc/9.3.0

module unload cray-mpich/7.7.10
module load openmpi

#spack load -r mochi-colza@main%gcc@9.3.0
#spack load -r mochi-colza@main+drc+examples%gcc@9.3.0
#spack load cppunit
#spack load -r mochi-colza@main+drc+examples%gcc@9.3.0 ^nlohmann-json@3.9.1 ^mochi-ssg@main ^mochi-bedrock@main
#spack install mochi-colza@debug+drc+examples%gcc@9.3.0 build_type=Debug ^nlohmann-json@3.9.1 ^mochi-ssg@main+valgrind cflags="-g" ^mochi-bedrock@main build_type=Debug ^mochi-mona build_type=Debug
#spack load -r mochi-colza@dynamicsim+drc+examples%gcc@9.3.0 ^nlohmann-json@3.9.1 ^mochi-ssg@main+valgrind cflags="-g" ^mochi-bedrock@main

spack load -r mochi-colza@debug+drc+examples%gcc@9.3.0 ^nlohmann-json@3.9.1 ^mochi-ssg@main+valgrind cflags="-g" ^mochi-bedrock@main
spack load spdlog%gcc@9.3.0

cd $SCRATCH/build_monavtk

export MPICH_GNI_NDREG_ENTRIES=1024
# get more mercury info
export HG_NA_LOG_LEVEL=debug

# try to avoid the argobot stack size issue
export ABT_THREAD_STACKSIZE=2097152