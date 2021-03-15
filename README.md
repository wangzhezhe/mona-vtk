## mona-vtk examples

This repo shows how to implement the MonaController and use it for Paraview Catalyst to do the in-situ data analytics. The `src` folder contains the implementation details of the MonaController based on the MonaCommunicator which is implemented based on [mochi-mona](https://github.com/mochi-hpc/mochi-mona).

There are several examples in the `example` folder:

- basic: This example shows that how the MonaController can be used to execute the basic vtk parallel operations such as send and recv vtk object.


- icetExample: This exmaple shows that how the mochi-mona can be used to execute the iceT test cases based on the iceT wrapper for the mochi-mona.


- MandelbulbCatalystExample: This example shows how the MonaController can be used to execute the tightly coupled in-situ analytics in distributed way.


- MandelbulbColza: This example shows how the MonaController can be used to execute the loosely coupled in-situ analytics in distributed way, the [mochi-colza](https://github.com/mochi-hpc/mochi-colza) is used as the data staging service for this example.


- GrayScottColza: This example is similar with the MandelbulbColza case but the data source is the Gray-Scott simulation.

## Installing

### Colza

When we install Colza, we also install the necessary mochi software stack

### Paraview

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

The example to install the colza with the exmaple and drc

```
mochi-colza@main+drc+examples
```

# Build

this is an example about the configuration on cori

```
#!/bin/bash
source ~/.color
#use the system default python, which is 3.6
#or refer to the python configuration of cori
module load spack
module load cmake/3.18.2
# for compiling vtk on cori
export CRAYPE_LINK_TYPE=dynamic

# let cc and CC to be the gnu compier
module swap PrgEnv-intel PrgEnv-gnu

# make sure the package.yaml has the same compiler version
module swap gcc/8.3.0 gcc/9.3.0
spack load -r mochi-colza@main%gcc@9.3.0
#spack load cppunit
#important! make sure the cc can find the correct linker, the spack may load the wrong linker
#there are still issues after loading the mesa not sure the reason
#spack load -r mesa/qozjngg
#PATH="/global/common/cori/software/altd/2.0/bin:$PATH"

cd $SCRATCH/build_monavtk

export MPICH_GNI_NDREG_ENTRIES=1024
# get more mercury info
export HG_NA_LOG_LEVEL=debug

# try to avoid the argobot stack size issue, set it to 2M
export ABT_THREAD_STACKSIZE=2097152
```

The mona example with the debug option

```
spack install mochi-mona build_type=Debug ^mercury  build_type=Debug
```

cmake example:

```
cmake ~/cworkspace/src/mona-vtk/ -DCMAKE_CXX_COMPILER=CC -DCMAKE_C_COMPILER=cc -DVTK_DIR=$SCRATCH/build_paraview_matthieu_release/ -DENABLE_EXAMPLE=ON -DParaView_DIR=$SCRATCH/build_paraview_matthieu_release/ -DBUILD_SHARED_LIBS=ON 
```

# Other potential issues

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

Try to build the paraview with the Release mode, otherwise, there are mosa related issues

For the python on cori, refer to this (https://docs.nersc.gov/development/languages/python/nersc-python/)
If you only use the module option, but the python is not the default one, there are some issues
One issue is "unnamed python module encoding", or other issues that have different gcc version which may cause the byte code issue
It is prefered to use the conda activate than the python virtual env 