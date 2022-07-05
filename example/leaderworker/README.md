
Examples in this folder show how to use the leader/worker mechanism to support the elasticity for both simulation and data staging service.
These examples also evaluate the elasticity policies for guiding the resource rescaling operations.
There are three key scripts in this folder:

### elasticity_mb_join_comparison.scripts

This script aims to evaluate how to add new processes into the data staging service.
The simulation used in this script is the `elasticmbstage.cpp`
We may choose the proper strategy (static or adaptive) at line 203, then recompile the code for evaluation.

### elasticity_mb_redistribute.scripts

This script aims to evaluate the efficiency of the elasticity policy to switch the process between the simulation and data staging service. We use synthetic data analytics in this evaluation.

### elasticity_mb_redistribute_vtkwritepipeline.scripts

This script aims to evaluate the efficiency of the elasticity policy to switch the process between the simulation and data staging service based on the vtkwritepipeline. We need to make sure the function `this->m_stagecommon_meta->m_pipeline->executesynthetic2` is 
adopted for the evaluate function at the ./pipeline/StagingProvider.hpp.

Besides, the simulation uses the `elasticmbleaderworker.cpp`, and we need to make sure the proper strategy is adopted. For example, if we try to evaluate the static strategy, we can use the `controller.naiveLeave2` function in the 
`elasticmbleaderworker.cpp`, if we try to evaluate the adaptive strategy, we can use the `controller.dynamicLeave` function.

### dwater example

The scripts in `dwaterclient/scripts/` listed scripts for running the analtyics with the dwater simulation. `testrunElasticnaive.scripts` is the script that adopts the naive strategy and `testrunElasticadaptive.scripts` is the script that adopts the adaptive elasticity strategy.


### install from the spack

```
git clone https://github.com/spack/spack.git
spack env create colza-env spack.yaml
spack env activate colza-env
spack repo add sw/mochi-spack-packages
spack find compilers
spack install
```