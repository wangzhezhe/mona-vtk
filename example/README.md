

### basic

```
srun -C haswell -n 2 ./example/basic/test_vtk_send_recv
```


### icetExample

```
srun -C haswell -n 4 ./example/icetExample/icet_simple
```
neet to update `g_num_tiles_x` and `g_num_tiles_y` if update the proc number


### mandelbulbCatalyst

for the gridwrite script (4 partitions and 10 steps)

```
srun -C haswell -n 4 ./example/MandelbulbCatalystExample/MandelbulbDynamic ~/cworkspace/src/mona-vtk/example/MandelbulbCatalystExample/scripts/gridwriter.py 4 0 10
```

for the render script (4 partitions and 10 steps)

```
srun -C haswell -n 4 ./example/MandelbulbCatalystExample/MandelbulbDynamic ~/cworkspace/src/mona-vtk/example/MandelbulbCatalystExample/scripts/render.py 4 0 10
```
