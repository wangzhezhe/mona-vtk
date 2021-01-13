# Dependencies

MonaVTK requires dedicated ParaView 5.8 or greater, cmake, mona, MPI.
On a linux workstation with Spack, the following will install the required dependencies.

the paraview patch:
https://gitlab.kitware.com/mdorier/paraview/-/tree/dev-icet-integration

build command

```
cmake ~/cworkspace/src/ParaView/ -DPARAVIEW_USE_QT=OFF -DPARAVIEW_USE_PYTHON=ON -DPARAVIEW_USE_MPI=ON -DVTK_OPENGL_HAS_OSMESA:BOOL=TRUE -DVTK_USE_X:BOOL=FALSE -DCMAKE_CXX_COMPILER=CC -DCMAKE_C_COMPILER=cc
```

other depedencies:

```
spack install mochi-mona@master
```

# Build

this is an example about the configuration on cori

```
#!/bin/bash
source ~/.color
module load python3
module load spack
module load cmake/3.18.2
# for compiling vtk on cori
export CRAYPE_LINK_TYPE=dynamic

# let cc and CC to be the gnu compier
module swap PrgEnv-intel PrgEnv-gnu
# use the gcc8.2.0, since osmesa use this version 
# keep sure allpackages are compiled by same gcc
# make sure the package.yaml has the same compiler version
module swap gcc/8.3.0 gcc/8.2.0
spack load -r mochi-colza@main%gcc@8.2.0
#spack load cppunit
#important! make sure the cc can find the correct linker, the spack may load the wrong linker
#there are still issues after loading the mesa not sure the reason
#spack load -r mesa/qozjngg
#PATH="/global/common/cori/software/altd/2.0/bin:$PATH"

cd $SCRATCH/build_monavtk
```

The mona example with the debug option

```
spack install mochi-mona build_type=Debug ^mercury  build_type=Debug
```

cmake example:

```
cmake ~/cworkspace/src/mona-vtk/ -DCMAKE_CXX_COMPILER=CC -DCMAKE_C_COMPILER=cc  -DVTK_DIR=$SCRATCH/build_paraview_matthieu/ -DENABLE_EXAMPLE=ON -DParaView_DIR=$SCRATCH/build_paraview_matthieu
```
