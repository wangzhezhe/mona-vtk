This example shows how to create a colza backend and store the mandelbulb data into the backend

debug build colza

spack install mochi-colza@main%gcc@8.2.0 build_type=Debug ^mochi-mona build_type=Debug

### example with the admin loader

srun -C haswell -l -n 4 -c 1 --cpu_bind=cores --mem-per-cpu=1000 gdb --args ./example/MandelbulbColza/mbserver -a ofi+tcp -s ssgfile -v trace

srun -C haswell -l -n 1 -c 1 --cpu_bind=cores --mem-per-cpu=1000 ./example/MandelbulbColza/mbadmin -a ofi+tcp -s ssgfile -x create -t monabackend -n monabackend -l /global/cscratch1/sd/zw241/build_monavtk/example/MandelbulbColza/libmonabackend-pipeline.so

srun -C haswell -n 1 ./example/MandelbulbColza/mbclient -a ofi+tcp -s ssgfile -p monabackend -b 6 -t 10 -v trace

### example with config loader

**mona comm**
srun -C haswell -n 4 -c 4 --cpu_bind=cores --mem-per-cpu=1000 ./example/MandelbulbColza/mbserver -a ofi+tcp -s ssgfile -c /global/homes/z/zw241/cworkspace/src/mona-vtk/example/MandelbulbColza/pipeline/monaconfig.json -v trace -t 4

**MPI comm**
srun -C haswell -n 4 -c 4 --cpu_bind=cores --mem-per-cpu=1000 ./example/MandelbulbColza/mbserver -a ofi+tcp -s ssgfile -c /global/homes/z/zw241/cworkspace/src/mona-vtk/example/MandelbulbColza/pipeline/mpiconfig.json -v trace -t 4

**client** 

srun -C haswell -n 4 -c 1 --cpu_bind=cores ./example/MandelbulbColza/mbclient -a $PROTOCOL -s $SSGFILE -p mpibackend -b $BLOCKNUM -t $STEP

### potential issues

if we use one core, there might some problems for SSG to add new nodes when loading the .so by config

if we use the config file to load the .so and with multiple thread, there are some seg faults for execution (not sure the reason yet)

python issue

https://gitlab.kitware.com/paraview/paraview/-/issues/20086

for current patch paraview, it is ok for python3.6

if there is unnamed module encoding issue for paraview, try to use the default python, namely python3.6 on cori

try to read this if the python and cori are used

https://docs.nersc.gov/development/languages/python/overview/


### known issue in large scale

cori_weakscale_mona_128_1024.script
happens by chance
```
Iteration 0 starting
ICET,23:ERROR: Radix-kr received image with wrong size.
ICET,23:ERROR: Input buffers do not agree for compressed-compressed composite.
ICET,21:ERROR: Radix-kr received image with wrong size.
# NA -- error -- /global/cscratch1/sd/zw241/sw/spackbuild/spack-stage-mercury-2.0.0-2p6qjd5wa4upto4xbjsirykb2jmxesro/spack-src/src/na/na_ofi.c:4447
 # na_ofi_msg_recv_unexpected(): Attempting to use OP ID that was not completed
terminate called after throwing an instance of 'std::runtime_error'
  what():  not success for wait any
^[[0m^[[1m^[[91m
Loguru caught a signal: SIGABRT
^[[0m^[[0m^[[31mStack trace:
```
