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


meshio_to_vtk_type = meshio._vtk_common.meshio_to_vtk_type

logger = logging.getLogger(__name__)


colza_to_numpy_type = {
    Type.FLOAT32 : np.dtype(np.float32),
    Type.FLOAT64 : np.dtype(np.float64),
    Type.INT8    : np.dtype(np.int8),
    Type.INT16   : np.dtype(np.int16),
    Type.INT32   : np.dtype(np.int32),
    Type.INT64   : np.dtype(np.int64),
    Type.UINT8    : np.dtype(np.uint8),
    Type.UINT16   : np.dtype(np.uint16),
    Type.UINT32   : np.dtype(np.uint32),
    Type.UINT64   : np.dtype(np.uint64),
}
numpy_to_colza_type = {str(v): k for k, v in colza_to_numpy_type.items()}
numpy_to_colza_type['<f4'] = Type.FLOAT32


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

    cell_types = np.array([], dtype=np.ubyte)
    cell_offsets = np.array([], dtype=int)
    cell_conn = np.array([], dtype=int)

    for meshio_type, data in mesh.cells:
        vtk_type = meshio_to_vtk_type[meshio_type]
        ncells, npoints = data.shape
        cell_types = np.hstack(
            [cell_types, np.full(ncells, vtk_type, dtype=np.ubyte)]
        )
        offsets = len(cell_conn) + (1 + npoints) * np.arange(ncells, dtype=int)
        cell_offsets = np.hstack([cell_offsets, offsets])
        conn = np.hstack(
            [npoints * np.ones((ncells, 1), dtype=int), data]
        ).flatten()
        cell_conn = np.hstack([cell_conn, conn])

    output['cell_types'] = cell_types
    output['cell_offsets'] = cell_offsets
    output['cell_conn'] = cell_conn
    return output


def process_file(iteration, filename, stage_path, pipeline):
    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    size = comm.Get_size()
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
    for i in range(0, len(vtu_files)):
        if i % size == rank:
            output[i] = read_vtu_file(vtu_files[i])

    logger.info('Starting iteration {} in Colza pipeline'.format(iteration))
    pipeline.start(iteration)

    logger.info('Staging data in Colza pipeline')
    for block_id, data in output.items():
        for name, array in data.items():
            pipeline.stage(
                dataset_name=name,
                iteration=iteration,
                type=numpy_to_colza_type[str(array.dtype)],
                block_id=block_id,
                data=array)

    logger.info('Executing pipeline')
    pipeline.execute(iteration=iteration,
                     auto_cleanup=True)

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

    if MPI.COMM_WORLD.Get_rank() != 0:
        logger.setLevel(logging.ERROR)
    else:
        logger.setLevel(logging.DEBUG)

    with Engine('ofi+tcp', pymargo.server) as engine:
        pyssg.init()
        process(engine, filenames, ssg_file, provider_id, pipeline, stage_path)
        engine.finalize()
        pyssg.finalize()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Mini-app reloading Deep Water Impact datasets')
    parser.add_argument('--protocol', type=str, help='Mercury protocol to use')
    parser.add_argument('--ssg-file', type=str, help='SSG file for Colza')
    parser.add_argument('--provider-id', type=int, default=0, help='Provide id of Colza servers')
    parser.add_argument('--stage-path', type=str, default='.', help='Folder in which to stage uncompressed data')
    parser.add_argument('--pipeline', type=str, help='Name of the Colza pipeline to which to send data')
    parser.add_argument('files', type=str, nargs='+', help='Files to load')
    args = parser.parse_args()
    run(args)
