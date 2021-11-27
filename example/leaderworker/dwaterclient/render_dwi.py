
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
# paraview version 5.6.0
#--------------------------------------------------------------

from paraview.simple import *
from paraview import coprocessing

# ----------------------- CoProcessor definition -----------------------

def CreateCoProcessor():
  def _CreatePipeline(coprocessor, datadescription):
    class Pipeline:
      # state file generated using paraview version 5.6.0

      # ----------------------------------------------------------------
      # setup views used in the visualization
      # ----------------------------------------------------------------

      # trace generated using paraview version 5.6.0
      #
      # To ensure correct image size when batch processing, please search 
      # for and uncomment the line `# renderView*.ViewSize = [*,*]`

      #### disable automatic camera reset on 'Show'
      paraview.simple._DisableFirstRenderCameraReset()

      # get the material library
      materialLibrary1 = GetMaterialLibrary()

      # Create a new 'Render View'
      renderView1 = CreateView('RenderView')
      renderView1.ViewSize = [1432, 818]
      renderView1.AxesGrid = 'GridAxes3DActor'
      renderView1.CenterOfRotation = [45000.0, 46250.0, 1250.0]
      renderView1.StereoType = 0
      renderView1.CameraPosition = [-1010572.3945121744, 842698.9608742782, 866637.8364272084]
      renderView1.CameraFocalPoint = [45000.00000000007, 46249.99999999987, 1250.0000000000255]
      renderView1.CameraViewUp = [0.23569368120409503, 0.8411835885885475, -0.4866812703708205]
      renderView1.CameraParallelScale = 724605.4961149549
      renderView1.Background = [0.32, 0.34, 0.43]
      renderView1.OSPRayMaterialLibrary = materialLibrary1

      # init the 'GridAxes3DActor' selected for 'AxesGrid'
      renderView1.AxesGrid.XTitleFontFile = ''
      renderView1.AxesGrid.YTitleFontFile = ''
      renderView1.AxesGrid.ZTitleFontFile = ''
      renderView1.AxesGrid.XLabelFontFile = ''
      renderView1.AxesGrid.YLabelFontFile = ''
      renderView1.AxesGrid.ZLabelFontFile = ''

      # register the view with coprocessor
      # and provide it with information such as the filename to use,
      # how frequently to write the images, etc.
      coprocessor.RegisterView(renderView1,
          filename='RenderView1_%t.png', freq=1, fittoscreen=0, magnification=1, width=1432, height=818, cinema={})
      renderView1.ViewTime = datadescription.GetTime()

      # ----------------------------------------------------------------
      # restore active view
      SetActiveView(renderView1)
      # ----------------------------------------------------------------

      # ----------------------------------------------------------------
      # setup the data processing pipelines
      # ----------------------------------------------------------------

      # create a new 'XML MultiBlock Data Reader'
      # create a producer from a simulation input
      _input = coprocessor.CreateProducer(datadescription, 'input')

      # create a new 'Merge Blocks'
      mergeBlocks1 = MergeBlocks(Input=_input)

      # create a new 'Iso Volume'
      isoVolume1 = IsoVolume(Input=mergeBlocks1)
      isoVolume1.InputScalars = ['CELLS', 'rho']
      isoVolume1.ThresholdRange = [0.05245817762855723, 0.9861517825015244]

      # ----------------------------------------------------------------
      # setup the visualization in view 'renderView1'
      # ----------------------------------------------------------------

      # show data from isoVolume1
      isoVolume1Display = Show(isoVolume1, renderView1)

      # get color transfer function/color map for 'v02'
      v02LUT = GetColorTransferFunction('v02')
      v02LUT.RGBPoints = [0.011709708720445633, 0.231373, 0.298039, 0.752941, 0.5058548543602228, 0.865003, 0.865003, 0.865003, 1.0, 0.705882, 0.0156863, 0.14902]
      v02LUT.ScalarRangeInitialized = 1.0

      # get opacity transfer function/opacity map for 'v02'
      v02PWF = GetOpacityTransferFunction('v02')
      v02PWF.Points = [0.011709708720445633, 0.0, 0.5, 0.0, 1.0, 1.0, 0.5, 0.0]
      v02PWF.ScalarRangeInitialized = 1

      # trace defaults for the display properties.
      isoVolume1Display.Representation = 'Surface'
      isoVolume1Display.ColorArrayName = ['CELLS', 'v02']
      isoVolume1Display.LookupTable = v02LUT
      isoVolume1Display.OSPRayScaleFunction = 'PiecewiseFunction'
      isoVolume1Display.SelectOrientationVectors = 'None'
      isoVolume1Display.ScaleFactor = 99250.0
      isoVolume1Display.SelectScaleArray = 'None'
      isoVolume1Display.GlyphType = 'Arrow'
      isoVolume1Display.GlyphTableIndexArray = 'None'
      isoVolume1Display.GaussianRadius = 4962.5
      isoVolume1Display.SetScaleArray = [None, '']
      isoVolume1Display.ScaleTransferFunction = 'PiecewiseFunction'
      isoVolume1Display.OpacityArray = [None, '']
      isoVolume1Display.OpacityTransferFunction = 'PiecewiseFunction'
      isoVolume1Display.DataAxesGrid = 'GridAxesRepresentation'
      isoVolume1Display.SelectionCellLabelFontFile = ''
      isoVolume1Display.SelectionPointLabelFontFile = ''
      isoVolume1Display.PolarAxes = 'PolarAxesRepresentation'
      isoVolume1Display.ScalarOpacityFunction = v02PWF
      isoVolume1Display.ScalarOpacityUnitDistance = 19601.911330059516

      # init the 'GridAxesRepresentation' selected for 'DataAxesGrid'
      isoVolume1Display.DataAxesGrid.XTitleFontFile = ''
      isoVolume1Display.DataAxesGrid.YTitleFontFile = ''
      isoVolume1Display.DataAxesGrid.ZTitleFontFile = ''
      isoVolume1Display.DataAxesGrid.XLabelFontFile = ''
      isoVolume1Display.DataAxesGrid.YLabelFontFile = ''
      isoVolume1Display.DataAxesGrid.ZLabelFontFile = ''

      # init the 'PolarAxesRepresentation' selected for 'PolarAxes'
      isoVolume1Display.PolarAxes.PolarAxisTitleFontFile = ''
      isoVolume1Display.PolarAxes.PolarAxisLabelFontFile = ''
      isoVolume1Display.PolarAxes.LastRadialAxisTextFontFile = ''
      isoVolume1Display.PolarAxes.SecondaryRadialAxesTextFontFile = ''

      # setup the color legend parameters for each legend in this view

      # get color legend/bar for v02LUT in view renderView1
      v02LUTColorBar = GetScalarBar(v02LUT, renderView1)
      v02LUTColorBar.Title = 'v02'
      v02LUTColorBar.ComponentTitle = ''
      v02LUTColorBar.TitleFontFile = ''
      v02LUTColorBar.LabelFontFile = ''

      # set color bar visibility
      v02LUTColorBar.Visibility = 1

      # show color legend
      isoVolume1Display.SetScalarBarVisibility(renderView1, True)

      # ----------------------------------------------------------------
      # setup color maps and opacity mapes used in the visualization
      # note: the Get..() functions create a new object, if needed
      # ----------------------------------------------------------------

      # ----------------------------------------------------------------
      # finally, restore active source
      SetActiveSource(isoVolume1)
      # ----------------------------------------------------------------
    return Pipeline()

  class CoProcessor(coprocessing.CoProcessor):
    def CreatePipeline(self, datadescription):
      self.Pipeline = _CreatePipeline(self, datadescription)

  coprocessor = CoProcessor()
  # these are the frequencies at which the coprocessor updates.
  freqs = {'input': [1, 1, 1]}
  coprocessor.SetUpdateFrequencies(freqs)
  if requestSpecificArrays:
    arrays = [['rho', 1], ['v02', 1]]
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
