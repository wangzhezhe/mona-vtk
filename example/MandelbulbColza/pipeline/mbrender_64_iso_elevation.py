
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
      renderView1.ViewSize = [2107, 1093]
      renderView1.AxesGrid = 'GridAxes3DActor'
      renderView1.CenterOfRotation = [32.0, 31.5, 31.5]
      renderView1.StereoType = 'Crystal Eyes'
      renderView1.CameraPosition = [113.06719598460907, 85.06135082862514, 101.28165387013907]
      renderView1.CameraFocalPoint = [32.0, 31.5, 31.5]
      renderView1.CameraViewUp = [0.40167756135935034, 0.4390788376230128, -0.8036572099172815]
      renderView1.CameraFocalDisk = 1.0
      renderView1.CameraParallelScale = 54.849794894785155
      renderView1.BackEnd = 'OSPRay raycaster'
      renderView1.OSPRayMaterialLibrary = materialLibrary1

      # register the view with coprocessor
      # and provide it with information such as the filename to use,
      # how frequently to write the images, etc.
      coprocessor.RegisterView(renderView1,
          filename='RenderView1_%t.png', freq=1, fittoscreen=0, magnification=1, width=1100, height=1100, cinema={}, compression=5)
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

      # create a new 'XML MultiBlock Data Reader'
      # create a producer from a simulation input
      input_1 = coprocessor.CreateProducer(datadescription, 'input')

      # create a new 'Contour'
      contour2 = Contour(Input=input)
      contour2.ContourBy = ['POINTS', 'mandelbulb']
      contour2.Isosurfaces = [90.0]
      contour2.PointMergeMethod = 'Uniform Binning'

      # create a new 'Elevation'
      elevation1 = Elevation(Input=contour2)
      elevation1.LowPoint = [8.058118542819855, 6.775534540381003, 0.32205270393102126]
      elevation1.HighPoint = [44.42810445271364, 48.23680250415515, 76.06345285652087]

      # create a new 'Contour'
      contour1 = Contour(Input=input_1)
      contour1.ContourBy = ['POINTS', 'grayscottu']
      contour1.Isosurfaces = [0.5918336000470292, 1.0, 0.8, 0.6, 0.4, 0.2]
      contour1.PointMergeMethod = 'Uniform Binning'

      # create a new 'Extract Surface'
      extractSurface1 = ExtractSurface(Input=contour1)

      # create a new 'Clip'
      clip1 = Clip(Input=contour1)
      clip1.ClipType = 'Plane'
      clip1.HyperTreeGridClipper = 'Plane'
      clip1.Scalars = ['POINTS', 'grayscottu']
      clip1.Value = 0.5918335914611816

      # init the 'Plane' selected for 'ClipType'
      clip1.ClipType.Origin = [31.49303436279297, 31.492342948913574, 31.49195957183838]

      # init the 'Plane' selected for 'HyperTreeGridClipper'
      clip1.HyperTreeGridClipper.Origin = [31.49303436279297, 31.492342948913574, 31.49195957183838]

      # ----------------------------------------------------------------
      # setup the visualization in view 'renderView1'
      # ----------------------------------------------------------------

      # show data from elevation1
      elevation1Display = Show(elevation1, renderView1, 'GeometryRepresentation')

      # get color transfer function/color map for 'Elevation'
      elevationLUT = GetColorTransferFunction('Elevation')
      elevationLUT.RGBPoints = [0.13094767928123474, 0.231373, 0.298039, 0.752941, 0.561008408665657, 0.865003, 0.865003, 0.865003, 0.9910691380500793, 0.705882, 0.0156863, 0.14902]
      elevationLUT.ScalarRangeInitialized = 1.0

      # trace defaults for the display properties.
      elevation1Display.Representation = 'Surface'
      elevation1Display.ColorArrayName = ['POINTS', 'Elevation']
      elevation1Display.LookupTable = elevationLUT
      elevation1Display.OSPRayScaleArray = 'Elevation'
      elevation1Display.OSPRayScaleFunction = 'PiecewiseFunction'
      elevation1Display.SelectOrientationVectors = 'Elevation'
      elevation1Display.ScaleFactor = 5.8206184864044195
      elevation1Display.SelectScaleArray = 'Elevation'
      elevation1Display.GlyphType = 'Arrow'
      elevation1Display.GlyphTableIndexArray = 'Elevation'
      elevation1Display.GaussianRadius = 0.29103092432022093
      elevation1Display.SetScaleArray = ['POINTS', 'Elevation']
      elevation1Display.ScaleTransferFunction = 'PiecewiseFunction'
      elevation1Display.OpacityArray = ['POINTS', 'Elevation']
      elevation1Display.OpacityTransferFunction = 'PiecewiseFunction'
      elevation1Display.DataAxesGrid = 'GridAxesRepresentation'
      elevation1Display.PolarAxes = 'PolarAxesRepresentation'

      # init the 'PiecewiseFunction' selected for 'ScaleTransferFunction'
      elevation1Display.ScaleTransferFunction.Points = [0.20397280156612396, 0.0, 0.5, 0.0, 0.8259249329566956, 1.0, 0.5, 0.0]

      # init the 'PiecewiseFunction' selected for 'OpacityTransferFunction'
      elevation1Display.OpacityTransferFunction.Points = [0.20397280156612396, 0.0, 0.5, 0.0, 0.8259249329566956, 1.0, 0.5, 0.0]

      # setup the color legend parameters for each legend in this view

      # get color legend/bar for elevationLUT in view renderView1
      elevationLUTColorBar = GetScalarBar(elevationLUT, renderView1)
      elevationLUTColorBar.Title = 'Elevation'
      elevationLUTColorBar.ComponentTitle = ''

      # set color bar visibility
      elevationLUTColorBar.Visibility = 1

      # show color legend
      elevation1Display.SetScalarBarVisibility(renderView1, True)

      # ----------------------------------------------------------------
      # setup color maps and opacity mapes used in the visualization
      # note: the Get..() functions create a new object, if needed
      # ----------------------------------------------------------------

      # get opacity transfer function/opacity map for 'Elevation'
      elevationPWF = GetOpacityTransferFunction('Elevation')
      elevationPWF.Points = [0.13094767928123474, 0.0, 0.5, 0.0, 0.9910691380500793, 1.0, 0.5, 0.0]
      elevationPWF.ScalarRangeInitialized = 1

      # ----------------------------------------------------------------
      # finally, restore active source
      SetActiveSource(contour2)
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
