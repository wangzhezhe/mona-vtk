import sys
import shutil
import os
import meshio
import argparse

import numpy as np

import glob
import re

import logging
import tarfile

logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)


meshio_to_vtk_type = meshio._vtk_common.meshio_to_vtk_type


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
        #print(meshio_type)
        vtk_type = meshio_to_vtk_type[meshio_type]
        ncells, npoints = data.shape
        #print (data.shape)
        #print("ncells",ncells)
        #print("npoints",npoints)

        #cell_types = np.hstack(
        #    [cell_types, np.full(ncells, vtk_type, dtype=np.ubyte)]
        #)
        #print(len(cell_conn))
        #offsets = len(cell_conn) + (npoints) * np.arange(ncells+1, dtype=np.int64)
        #print(offsets)

        #cell_offsets = np.hstack([cell_offsets, offsets])
        #print(cell_offsets)
        
        #conn = data.flatten()
        #print(np.ones((ncells, 1)).shape)
        #print(conn[:10])
        #cell_conn = np.hstack([cell_conn, conn])
        #rint(cell_conn)

    #print("cell_types shape", cell_types.shape)
    #print("cell_offsets shape", cell_offsets.shape)
    #print("cell_conn shape", cell_conn.shape)

    #output['cell_types'] = cell_types
    #output['cell_offsets'] = cell_offsets
    #output['cell_conn'] = cell_conn
    return ncells

def run(filename):
    stage_path='.'
    name = filename.split('/')[-1].split('.')[-2]
    to_remove = []
    logger.info('Untarring file {} into folder {}'.format(filename, stage_path))
    tar = tarfile.open(filename, mode='r')
    to_remove = [stage_path+'/'+name for name in tar.getnames() if '/' not in name]
    tar.extractall(stage_path)

    vtm_file = find_vtm_file(stage_path)
    vtu_files = list_vtu_files_from_vtm(vtm_file)
    vtu_files = [ stage_path+'/'+f for f in vtu_files]

    totalncell=0
    for i in range(0, len(vtu_files)):
        ncell=read_vtu_file(vtu_files[i])
        totalncell+=ncell
    print(filename,"total cells",totalncell)
    to_remove = []
    to_remove = [stage_path+'/'+name for name in tar.getnames() if '/' not in name]
    for f in to_remove:
        try:
            os.remove(f)
        except OSError:
            shutil.rmtree(f)

if __name__ == '__main__':
    filenames=sys.argv[1:]
    #print(filenames)
    for i, filename in enumerate(filenames):
        #print(filename)
        run(filename)


