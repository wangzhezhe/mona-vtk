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
spack load -r mesa/qozjngg
spack load -r mochi-mona@master

# let cc and CC to be the gnu compier
module swap PrgEnv-intel PrgEnv-gnu

#important! make sure the cc can find the correct linker, the spack may load the wrong linker
PATH="/global/common/cori/software/altd/2.0/bin:$PATH"

cd $SCRATCH/build_monavtk
```

cmake example:

```
cmake ~/cworkspace/src/mona-vtk/ -DCMAKE_CXX_COMPILER=CC -DCMAKE_C_COMPILER=cc  -DVTK_DIR=$SCRATCH/build_paraview_matthieu/ -DENABLE_EXAMPLE=ON -DParaView_DIR=$SCRATCH/build_paraview_matthieu
```
