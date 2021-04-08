
#--------------------------------------------------------------

# Global timestep output options
timeStepToStartOutputAt=0
forceOutputAtFirstCall=False

# Global screenshot output options
imageFileNamePadding=0
rescale_lookuptable=False

# Whether or not to request specific arrays from the adaptor.
requestSpecificArrays=False

# a root directory under which all Catalyst output goes
rootDirectory=''

# makes a cinema D index table
make_cinema_table=False

#--------------------------------------------------------------
# Code generated from cpstate.py to create the CoProcessor.
# paraview version 5.8.0
#--------------------------------------------------------------

from paraview.simple import *
from paraview import coprocessing

# ----------------------- CoProcessor definition -----------------------

def CreateCoProcessor():
  def _CreatePipeline(coprocessor, datadescription):
    class Pipeline:
      # state file generated using paraview version 5.8.0

      # ----------------------------------------------------------------
      # setup views used in the visualization
      # ----------------------------------------------------------------

      # trace generated using paraview version 5.8.0
      #
      # To ensure correct image size when batch processing, please search 
      # for and uncomment the line `# renderView*.ViewSize = [*,*]`

      #### disable automatic camera reset on 'Show'
      paraview.simple._DisableFirstRenderCameraReset()

      # get the material library
      materialLibrary1 = GetMaterialLibrary()

      # Create a new 'Render View'
      renderView1 = CreateView('RenderView')
      renderView1.ViewSize = [1358, 1170]
      renderView1.AxesGrid = 'GridAxes3DActor'
      renderView1.CenterOfRotation = [31.5, 31.5, 31.5]
      renderView1.StereoType = 'Crystal Eyes'
      renderView1.CameraPosition = [734.6407991434337, -700.1610574332983, 1022.1939960923457]
      renderView1.CameraFocalPoint = [31.50000000000002, 31.500000000000057, 31.499999999999986]
      renderView1.CameraViewUp = [0.6467743172859446, 0.7561743370484042, 0.09941506167933858]
      renderView1.CameraFocalDisk = 1.0
      renderView1.CameraParallelScale = 54.559600438419636
      renderView1.BackEnd = 'OSPRay raycaster'
      renderView1.OSPRayMaterialLibrary = materialLibrary1

      # register the view with coprocessor
      # and provide it with information such as the filename to use,
      # how frequently to write the images, etc.
      coprocessor.RegisterView(renderView1,
          filename='RenderView1_%t.png', freq=1, fittoscreen=0, magnification=1, width=1110, height=1110, cinema={}, compression=5)
      renderView1.ViewTime = datadescription.GetTime()

      SetActiveView(None)

      # ----------------------------------------------------------------
      # setup view layouts
      # ----------------------------------------------------------------

      # create new layout object 'Layout #1'
      layout1 = CreateLayout(name='Layout #1')
      layout1.AssignView(0, renderView1)

      # ----------------------------------------------------------------
      # restore active view
      SetActiveView(renderView1)
      # ----------------------------------------------------------------

      # ----------------------------------------------------------------
      # setup the data processing pipelines
      # ----------------------------------------------------------------

      # create a new 'XML MultiBlock Data Reader'
      # create a producer from a simulation input
      input = coprocessor.CreateProducer(datadescription, 'input')

      # create a new 'Contour'
      contour1 = Contour(Input=input)
      contour1.ContourBy = ['POINTS', 'grayscottu']
      contour1.Isosurfaces = [0.8]
      contour1.PointMergeMethod = 'Uniform Binning'

      # ----------------------------------------------------------------
      # setup the visualization in view 'renderView1'
      # ----------------------------------------------------------------

      # show data from contour1
      contour1Display = Show(contour1, renderView1, 'GeometryRepresentation')

      # get color transfer function/color map for 'grayscottu'
      grayscottuLUT = GetColorTransferFunction('grayscottu')
      grayscottuLUT.RGBPoints = [0.22777995149045788, 0.231373, 0.298039, 0.752941, 0.6188899428167411, 0.865003, 0.865003, 0.865003, 1.0099999341430244, 0.705882, 0.0156863, 0.14902]
      grayscottuLUT.ScalarRangeInitialized = 1.0

      # trace defaults for the display properties.
      contour1Display.Representation = 'Surface'
      contour1Display.ColorArrayName = ['POINTS', 'grayscottu']
      contour1Display.LookupTable = grayscottuLUT
      contour1Display.OSPRayScaleArray = 'grayscottu'
      contour1Display.OSPRayScaleFunction = 'PiecewiseFunction'
      contour1Display.SelectOrientationVectors = 'None'
      contour1Display.ScaleFactor = 1.6030078887939454
      contour1Display.SelectScaleArray = 'grayscottu'
      contour1Display.GlyphType = 'Arrow'
      contour1Display.GlyphTableIndexArray = 'grayscottu'
      contour1Display.GaussianRadius = 0.08015039443969726
      contour1Display.SetScaleArray = ['POINTS', 'grayscottu']
      contour1Display.ScaleTransferFunction = 'PiecewiseFunction'
      contour1Display.OpacityArray = ['POINTS', 'grayscottu']
      contour1Display.OpacityTransferFunction = 'PiecewiseFunction'
      contour1Display.DataAxesGrid = 'GridAxesRepresentation'
      contour1Display.PolarAxes = 'PolarAxesRepresentation'

      # init the 'PiecewiseFunction' selected for 'ScaleTransferFunction'
      contour1Display.ScaleTransferFunction.Points = [0.6188899278640747, 0.0, 0.5, 0.0, 0.6190119981765747, 1.0, 0.5, 0.0]

      # init the 'PiecewiseFunction' selected for 'OpacityTransferFunction'
      contour1Display.OpacityTransferFunction.Points = [0.6188899278640747, 0.0, 0.5, 0.0, 0.6190119981765747, 1.0, 0.5, 0.0]

      # setup the color legend parameters for each legend in this view

      # get color legend/bar for grayscottuLUT in view renderView1
      grayscottuLUTColorBar = GetScalarBar(grayscottuLUT, renderView1)
      grayscottuLUTColorBar.Title = 'grayscottu'
      grayscottuLUTColorBar.ComponentTitle = ''

      # set color bar visibility
      grayscottuLUTColorBar.Visibility = 1

      # show color legend
      contour1Display.SetScalarBarVisibility(renderView1, True)

      # ----------------------------------------------------------------
      # setup color maps and opacity mapes used in the visualization
      # note: the Get..() functions create a new object, if needed
      # ----------------------------------------------------------------

      # get opacity transfer function/opacity map for 'grayscottu'
      grayscottuPWF = GetOpacityTransferFunction('grayscottu')
      grayscottuPWF.Points = [0.22777995149045788, 0.0, 0.5, 0.0, 1.0099999341430244, 1.0, 0.5, 0.0]
      grayscottuPWF.ScalarRangeInitialized = 1

      # ----------------------------------------------------------------
      # finally, restore active source
      SetActiveSource(contour1)
      # ----------------------------------------------------------------
    return Pipeline()

  class CoProcessor(coprocessing.CoProcessor):
    def CreatePipeline(self, datadescription):
      self.Pipeline = _CreatePipeline(self, datadescription)

  coprocessor = CoProcessor()
  # these are the frequencies at which the coprocessor updates.
  freqs = {'input': [1]}
  coprocessor.SetUpdateFrequencies(freqs)
  if requestSpecificArrays:
    arrays = [['grayscottu', 0]]
    coprocessor.SetRequestedArrays('input', arrays)
  coprocessor.SetInitialOutputOptions(timeStepToStartOutputAt,forceOutputAtFirstCall)

  if rootDirectory:
      coprocessor.SetRootDirectory(rootDirectory)

  if make_cinema_table:
      coprocessor.EnableCinemaDTable()

  return coprocessor


#--------------------------------------------------------------
# Global variable that will hold the pipeline for each timestep
# Creating the CoProcessor object, doesn't actually create the ParaView pipeline.
# It will be automatically setup when coprocessor.UpdateProducers() is called the
# first time.
coprocessor = CreateCoProcessor()

#--------------------------------------------------------------
# Enable Live-Visualizaton with ParaView and the update frequency
coprocessor.EnableLiveVisualization(False, 1)

# ---------------------- Data Selection method ----------------------

def RequestDataDescription(datadescription):
    "Callback to populate the request for current timestep"
    global coprocessor

    # setup requests for all inputs based on the requirements of the
    # pipeline.
    coprocessor.LoadRequestedData(datadescription)

# ------------------------ Processing method ------------------------

def DoCoProcessing(datadescription):
    "Callback to do co-processing for current timestep"
    global coprocessor

    # Update the coprocessor by providing it the newly generated simulation data.
    # If the pipeline hasn't been setup yet, this will setup the pipeline.
    coprocessor.UpdateProducers(datadescription)

    # Write output data, if appropriate.
    coprocessor.WriteData(datadescription);

    # Write image capture (Last arg: rescale lookup table), if appropriate.
    coprocessor.WriteImages(datadescription, rescale_lookuptable=rescale_lookuptable,
        image_quality=0, padding_amount=imageFileNamePadding)

    # Live Visualization, if enabled.
    coprocessor.DoLiveVisualization(datadescription, "localhost", 22222)
