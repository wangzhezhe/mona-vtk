import sys
import shutil
import os
import meshio
import argparse
from mpi4py import MPI
import pymargo
from pymargo.core import Engine
from pycolza.client import ColzaClient, ColzaCommunicator, Type
import pycolza.client
import pyssg
import numpy as np
import tarfile
import logging
import glob
import re
import time


meshio_to_vtk_type = meshio._vtk_common.meshio_to_vtk_type

logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)


colza_to_numpy_type = {
    Type.FLOAT32 : np.dtype(np.float32),
    Type.FLOAT64 : np.dtype(np.float64),
    Type.INT8    : np.dtype(np.int8),
    Type.INT16   : np.dtype(np.int16),
    Type.INT32   : np.dtype(np.int32),
    Type.INT64   : np.dtype(np.int64),
    Type.UINT8    : np.dtype(np.uint8), #byte is an alias for uint8
    Type.UINT16   : np.dtype(np.uint16),
    Type.UINT32   : np.dtype(np.uint32),
    Type.UINT64   : np.dtype(np.uint64),
}
numpy_to_colza_type = {str(v): k for k, v in colza_to_numpy_type.items()}
numpy_to_colza_type['<f4'] = Type.FLOAT32
avalible_process = 0

def find_vtm_file(path):
    files = glob.glob(path+'/*.vtm')
    if len(files) == 0:
        raise RuntimeError('Could not find .vtm file to read')
    logger.info('Found {} file'.format(files[0]))
    return files[0]


def list_vtu_files_from_vtm(vtm_file):
    vtu_files = []
    for line in open(vtm_file):
        if '<DataSet ' in line:
            m = re.search('file=\"([^\"]*)', line)
            vtu_files.append(m.group(1))
    return vtu_files


def read_vtu_file(vtu_file):
    """
    We do something similar to
    https://github.com/nschloe/meshio/blob/main/tools/paraview-meshio-plugin.py
    """
    print('Rank {} reading {}'.format(MPI.COMM_WORLD.Get_rank(), vtu_file))
    mesh = meshio.read(vtu_file)
    output = {}
    output['rho'] = mesh.cell_data['rho'][0]
    output['v02'] = mesh.cell_data['v02'][0]
    output['points'] = mesh.points

    #print ("rho type {} v02 type {} points array type {}".format(output['rho'].dtype,output['v02'].dtype,output['points'].dtype))

    cell_types = np.array([], dtype=np.ubyte)
    cell_offsets = np.array([], dtype=np.int64)
    cell_conn = np.array([], dtype=np.int64)

    for meshio_type, data in mesh.cells:
        vtk_type = meshio_to_vtk_type[meshio_type]
        ncells, npoints = data.shape

        cell_types = np.hstack(
            [cell_types, np.full(ncells, vtk_type, dtype=np.ubyte)]
        )
        offsets = len(cell_conn) + (npoints) * np.arange(ncells+1, dtype=np.int64)
        cell_offsets = np.hstack([cell_offsets, offsets])
        #conn = np.hstack(
        #    [npoints * np.ones((ncells, 1), dtype=np.int64), data]
        #).flatten()
        conn = data.flatten()
        cell_conn = np.hstack([cell_conn, conn])

    output['cell_types'] = cell_types
    output['cell_offsets'] = cell_offsets
    output['cell_conn'] = cell_conn
    return output


def process_file(iteration, filename, stage_path, pipeline):
    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    size = comm.Get_size()
    print (filename)
    name = filename.split('/')[-1].split('.')[-2]
    to_remove = []
    if rank == 0:
        logger.info('Untarring file {} into folder {}'.format(filename, stage_path))
        tar = tarfile.open(filename, mode='r')
        to_remove = [stage_path+'/'+name for name in tar.getnames() if '/' not in name]
        tar.extractall(stage_path)
    comm.Barrier()

    logger.info('Searching for VTM file')
    vtm_file = find_vtm_file(stage_path)
    logger.info('Listing VTU files')
    vtu_files = list_vtu_files_from_vtm(vtm_file)
    vtu_files = [ stage_path+'/'+f for f in vtu_files]

    logger.info('Getting data from VTU files')
    output = {}
    tstart = time.perf_counter()
    for i in range(0, len(vtu_files)):
        if i % size == rank:
            output[i] = read_vtu_file(vtu_files[i])

    logger.info('Starting iteration {} in Colza pipeline'.format(iteration))
    
    # write a file to tell the data staging service to start a new one
    # how to make sure the server process started successfully here?
    if rank==0 and iteration>=2:
        # when there is avalible process 
        global avalible_process
        if avalible_process > 0:
            #write a signal file
            signalfile = "dwiconfig_" + str(avalible_process)
            with open(signalfile, 'w') as f:
                data = 'test'
                f.write(data)
            print("write a signal file", signalfile)
            avalible_process = avalible_process-1

    pipeline.start(iteration)
    tend = time.perf_counter()
    print(f"rank {rank} file extracting time for iteration {iteration} filename {filename} takes {tend - tstart:0.4f} seconds")


    logger.info('Staging data in Colza pipeline')
    tstart = time.perf_counter()
    for block_id, data in output.items():
        for name, array in data.items():
            pipeline.stage(
                dataset_name=name,
                iteration=iteration,
                type=numpy_to_colza_type[str(array.dtype)],
                block_id=block_id,
                data=array)
    tend = time.perf_counter()
    if rank==0:
        print(f"Data staging time for iteration {iteration} filename {filename} takes {tend - tstart:0.4f} seconds")


    logger.info('Executing pipeline')

    tic = time.perf_counter()
    pipeline.execute(iteration=iteration,
                     auto_cleanup=True)
    toc = time.perf_counter()
    
    # only some of processes are scheduled to execute t
    print(f"rank {rank} Execution time for iteration {iteration} filename {filename} takes {toc - tic:0.4f} seconds")

    logger.info('Removing staged files')
    for f in to_remove:
        try:
            os.remove(f)
        except OSError:
            shutil.rmtree(f)


def process(engine, filenames, ssg_file, provider_id, pipeline_name, stage_path):
    colza_comm = ColzaCommunicator(MPI.COMM_WORLD)
    colza_client = ColzaClient(engine)

    colza_pipeline = colza_client.make_distributed_pipeline_handle(
        comm=colza_comm,
        ssg_file=ssg_file,
        provider_id=provider_id,
        name=pipeline_name,
        check=True)
    # the iteration number equals to the number of tar files
    for i, filename in enumerate(filenames):
        MPI.COMM_WORLD.Barrier()
        process_file(i, filename, stage_path, colza_pipeline)


def run(args):
    filenames = args.files
    protocol = args.protocol
    ssg_file = args.ssg_file
    pipeline = args.pipeline
    provider_id = args.provider_id
    stage_path = args.stage_path
    #this is a global variable
    global avalible_process
    avalible_process = args.elasticnum

    if MPI.COMM_WORLD.Get_rank() != 0:
        logger.setLevel(logging.ERROR)
    else:
        logger.setLevel(logging.INFO)
    
    cookie = pyssg.get_credentials_from_ssg_file(ssg_file)
    with Engine(protocol, pymargo.server, options={ 'mercury': { 'auth_key': cookie }}) as engine:
        process(engine, filenames, ssg_file, provider_id, pipeline, stage_path)
        engine.finalize()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Mini-app reloading Deep Water Impact datasets')
    parser.add_argument('--protocol', type=str, help='Mercury protocol to use')
    parser.add_argument('--ssg-file', type=str, help='SSG file for Colza')
    parser.add_argument('--provider-id', type=int, default=0, help='Provide id of Colza servers')
    parser.add_argument('--stage-path', type=str, default='.', help='Folder in which to stage uncompressed data')
    parser.add_argument('--pipeline', type=str, help='Name of the Colza pipeline to which to send data')
    parser.add_argument('--files', type=str, nargs='+', help='Files to load')
    parser.add_argument('--elasticnum', type=int, default='0', help='Process number that is elastic')
    args = parser.parse_args()
    pyssg.init()
    run(args)
    pyssg.finalize()

