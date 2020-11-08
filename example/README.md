

### basic

```
srun -C haswell -n 2 ./example/basic/test_vtk_send_recv
```


### icetExample

```
srun -C haswell -n 4 ./example/icetExample/icet_simple
```
neet to update `g_num_tiles_x` and `g_num_tiles_y` if update the proc number