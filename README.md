# Dependencies

MonaVTK requires dedicated ParaView 5.8 or greater, cmake, mona, MPI.
On a linux workstation with Spack, the following will install the required dependencies.

the paraview patch:
https://gitlab.kitware.com/mdorier/paraview/-/tree/dev-icet-integration

build command

```
cmake ~/cworkspace/src/ParaView_matthieu/paraview -DPARAVIEW_USE_QT=OFF -DPARAVIEW_USE_PYTHON=ON -DPARAVIEW_USE_MPI=ON -DVTK_OPENGL_HAS_OSMESA:BOOL=TRUE -DVTK_USE_X:BOOL=FALSE -DCMAKE_CXX_COMPILER=CC -DCMAKE_C_COMPILER=cc -DVTK_PYTHON_OPTIONAL_LINK=OFF
```

other depedencies:

```
spack install mochi-mona@master
```

For the osmesa, maybe try following command if it is not installed on the targeted platform

```
spack install mesa+osmesa~llvm swr=none
```

# Build

this is an example about the configuration on cori

```
#!/bin/bash
source ~/.color
#module load python3
module load python/3.7-anaconda-2019.10
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

# important!
export MPICH_GNI_NDREG_ENTRIES=1024
# get more mercury info
export HG_NA_LOG_LEVEL=debug
```

The mona example with the debug option

```
spack install mochi-mona build_type=Debug ^mercury  build_type=Debug
```

cmake example:

```
cmake ~/cworkspace/src/mona-vtk/ -DCMAKE_CXX_COMPILER=CC -DCMAKE_C_COMPILER=cc -DVTK_DIR=$SCRATCH/build_paraview_matthieu/ -DENABLE_EXAMPLE=ON -DParaView_DIR=$SCRATCH/build_paraview_matthieu -DBUILD_SHARED_LIBS=ON 
```

# other potential issues

https://discourse.paraview.org/t/undefined-symbol-pyexc-valueerror/5494/5


```
/usr/bin/ld: /global/common/sw/cray/sles15/x86_64/mesa/18.3.6/gcc/8.2.0/qozjngg/lib/libOSMesa.so: undefined reference to `del_curterm@NCURSES6_TINFO_5.0.19991023'
```
try this:

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ltinfo")

refer to

https://github.com/halide/Halide/issues/1112

if the MPICH_GNI_NDREG_ENTRIES is not set properly
https://github.com/mercury-hpc/mercury/issues/426

some osmesa warning from paraview if it is built in the Debug mode for building paraview (it is ok when we use the Release mode to build the paraview)

(  44.958s) [pvbatch.3       ]vtkOpenGLFramebufferObj:356    ERR| vtkOpenGLFramebufferObject (0x10005dc58e0): failed at glGenFramebuffers 1 OpenGL errors detected
1:   0 : (1280) Invalid enum

 vtkOpenGLState.cxx:505   WARN| Error glBindFramebuffer1 OpenGL errors detected
2:   0 : (1280) Invalid enum

