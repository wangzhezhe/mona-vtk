#!/bin/bash
#SBATCH --qos=debug
#SBATCH --nodes=6
#SBATCH --time=29:00
#SBATCH --licenses=cscratch1
#SBATCH --constraint=haswell


#necessary env
#margo related var
export MPICH_GNI_NDREG_ENTRIES=1024
# get more mercury info
export HG_NA_LOG_LEVEL=debug
# try to avoid the argobot stack size issue
export ABT_THREAD_STACKSIZE=2097152

export BUILDDIR=/global/cscratch1/sd/zw241/build_mona-vtk-matthieu
export SRCDIR=/global/homes/z/zw241/cworkspace/src/mona-vtk-matthieu/mona-vtk

export ABT_THREAD_STACKSIZE=2097152
export LD_LIBRARY_PATH=/global/common/sw/cray-sles15-x86_64/gcc-8.2.0/mesa-18.3.6-qozjngg/lib:$LD_LIBRARY_PATH


cd $BUILDDIR
# key envs
SSGFILE=ssgfile
PROTOCOL=gni

rm $SSGFILE

rm core
ulimit -c unlimited 

SEVERCONFIG=$SRCDIR/example/DeepWaterImpact/pipeline/DWIMonaConfig.json
RENDERSCRIPT=$SRCDIR/example/DeepWaterImpact/pipeline/render_dwi.py
cp $RENDERSCRIPT .

rm -rf pv_insitu_*
rm *.png

TARFILEPATH=/global/cscratch1/sd/zw241/DeepWaterImpact
#TARFILE=pv_insitu_15674.tar
#cp /global/cscratch1/sd/zw241/pv_insitu_15674_test_2/pv_insitu_15674.tar .
TARFILE="DeepWaterImpactData/pv_insitu_15674.tar \
DeepWaterImpactData/pv_insitu_16898.tar \
DeepWaterImpactData/pv_insitu_18432.tar \
DeepWaterImpactData/pv_insitu_19749.tar \
DeepWaterImpactData/pv_insitu_21255.tar \
DeepWaterImpactData/pv_insitu_22960.tar \
DeepWaterImpactData/pv_insitu_24647.tar \
DeepWaterImpactData/pv_insitu_26103.tar \
DeepWaterImpactData/pv_insitu_27405.tar \
DeepWaterImpactData/pv_insitu_28649.tar"

cp $SRCDIR/example/DeepWaterImpact/client/dwi_client.py .

SERVERNODE=4
CLIENTNODE=2

SERVERNUM=32
CLIENTNUM=32

rm dwiserver_mona_${SERVERNUM}.log
rm dwiclient_mona_${CLIENTNUM}_${SERVERNUM}.log

# 1 nodes for server (16*4=64=2*32)
# the colza-dist-server is in PATH after the associated env is loaded
srun -C haswell -N $SERVERNODE -l -n $SERVERNUM -c 4 --cpu_bind=cores colza-dist-server -a $PROTOCOL -s $SSGFILE -c $SEVERCONFIG -t 1 -v info &> dwiserver_mona_${SERVERNUM}.log &

#make sure the server load the pipeline
result=0
while [ $result -ne $SERVERNUM ]
do
    result=$(cat dwiserver_mona_"$SERVERNUM".log | grep "Server running at" | wc -l)
    echo "$result server loaded backend"
    sleep 1  
done

#load necessary package for python script
#use the tcp for temporary testing, use gni things in future
srun -C haswell -N $CLIENTNODE -n $CLIENTNUM -c 2 --cpu_bind=cores python3 dwi_client.py --protocol $PROTOCOL --ssg-file $SSGFILE --provider-id 0 --stage-path . --pipeline dwi_mona_backend --files $TARFILE &> dwiclient_mona_${CLIENTNUM}_${SERVERNUM}.log 

wait

