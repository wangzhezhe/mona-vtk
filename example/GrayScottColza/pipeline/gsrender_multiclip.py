
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
      renderView1.ViewSize = [2107, 1143]
      renderView1.AxesGrid = 'GridAxes3DActor'
      renderView1.CenterOfRotation = [31.49303436279297, 31.500001430511478, 31.500001430511478]
      renderView1.StereoType = 'Crystal Eyes'
      renderView1.CameraPosition = [168.9826538314206, -79.79845791950946, -87.7233329831918]
      renderView1.CameraFocalPoint = [31.49303436279297, 31.500001430511478, 31.500001430511478]
      renderView1.CameraViewUp = [0.6842454858236607, 0.7197818445849783, 0.11714184280515964]
      renderView1.CameraFocalDisk = 1.0
      renderView1.CameraParallelScale = 66.8052975190038
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

      # create a new 'Contour'
      contour1 = Contour(Input=input)
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

      # show data from clip1
      clip1Display = Show(clip1, renderView1, 'UnstructuredGridRepresentation')

      # get color transfer function/color map for 'grayscottu'
      grayscottuLUT = GetColorTransferFunction('grayscottu')
      grayscottuLUT.RGBPoints = [0.15895333278849172, 0.231373, 0.298039, 0.752941, 0.5918336000470292, 0.865003, 0.865003, 0.865003, 1.0247138673055667, 0.705882, 0.0156863, 0.14902]
      grayscottuLUT.ScalarRangeInitialized = 1.0

      # get opacity transfer function/opacity map for 'grayscottu'
      grayscottuPWF = GetOpacityTransferFunction('grayscottu')
      grayscottuPWF.Points = [0.15895333278849172, 0.0, 0.5, 0.0, 1.0247138673055667, 1.0, 0.5, 0.0]
      grayscottuPWF.ScalarRangeInitialized = 1

      # trace defaults for the display properties.
      clip1Display.Representation = 'Surface'
      clip1Display.ColorArrayName = ['POINTS', 'grayscottu']
      clip1Display.LookupTable = grayscottuLUT
      clip1Display.OSPRayScaleArray = 'grayscottu'
      clip1Display.OSPRayScaleFunction = 'PiecewiseFunction'
      clip1Display.SelectOrientationVectors = 'None'
      clip1Display.ScaleFactor = 1.762809181213379
      clip1Display.SelectScaleArray = 'grayscottu'
      clip1Display.GlyphType = 'Arrow'
      clip1Display.GlyphTableIndexArray = 'grayscottu'
      clip1Display.GaussianRadius = 0.08814045906066895
      clip1Display.SetScaleArray = ['POINTS', 'grayscottu']
      clip1Display.ScaleTransferFunction = 'PiecewiseFunction'
      clip1Display.OpacityArray = ['POINTS', 'grayscottu']
      clip1Display.OpacityTransferFunction = 'PiecewiseFunction'
      clip1Display.DataAxesGrid = 'GridAxesRepresentation'
      clip1Display.PolarAxes = 'PolarAxesRepresentation'
      clip1Display.ScalarOpacityFunction = grayscottuPWF
      clip1Display.ScalarOpacityUnitDistance = 2.1092016456432767
      clip1Display.ExtractedBlockIndex = 1

      # init the 'PiecewiseFunction' selected for 'ScaleTransferFunction'
      clip1Display.ScaleTransferFunction.Points = [0.5918335914611816, 0.0, 0.5, 0.0, 0.5919556617736816, 1.0, 0.5, 0.0]

      # init the 'PiecewiseFunction' selected for 'OpacityTransferFunction'
      clip1Display.OpacityTransferFunction.Points = [0.5918335914611816, 0.0, 0.5, 0.0, 0.5919556617736816, 1.0, 0.5, 0.0]

      # setup the color legend parameters for each legend in this view

      # get color legend/bar for grayscottuLUT in view renderView1
      grayscottuLUTColorBar = GetScalarBar(grayscottuLUT, renderView1)
      grayscottuLUTColorBar.Title = 'grayscottu'
      grayscottuLUTColorBar.ComponentTitle = ''

      # set color bar visibility
      grayscottuLUTColorBar.Visibility = 1

      # show color legend
      clip1Display.SetScalarBarVisibility(renderView1, True)

      # ----------------------------------------------------------------
      # setup color maps and opacity mapes used in the visualization
      # note: the Get..() functions create a new object, if needed
      # ----------------------------------------------------------------

      # ----------------------------------------------------------------
      # finally, restore active source
      SetActiveSource(clip1)
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
