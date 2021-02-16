
### using multiple thread

VTK doesn’t like that we are using multiple threads

related discussion

https://discourse.paraview.org/t/run-the-coprocess-function-by-a-separate-thread/5728)

### communicator issue

VTK doesn’t like that we change the communicator after it has been initialized

https://gitlab.kitware.com/paraview/paraview/-/merge_requests/4361/diffs

the key to Catalyst not hanging was to make sure the vtkIceTContext gets reinitialized when the controller is modified

https://gitlab.kitware.com/mdorier/paraview/-/tree/dev-icet-integration

