import sys
import shutil
import os
import meshio
import argparse

import numpy as np

import glob
import re


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

        cell_types = np.hstack(
            [cell_types, np.full(ncells, vtk_type, dtype=np.ubyte)]
        )
        #print(len(cell_conn))
        offsets = len(cell_conn) + (npoints) * np.arange(ncells+1, dtype=np.int64)
        #print(offsets)

        cell_offsets = np.hstack([cell_offsets, offsets])
        #print(cell_offsets)
        
        conn = data.flatten()
        #print(np.ones((ncells, 1)).shape)
        #print(conn[:10])
        cell_conn = np.hstack([cell_conn, conn])
        #rint(cell_conn)

    #print("cell_types shape", cell_types.shape)
    #print("cell_offsets shape", cell_offsets.shape)
    #print("cell_conn shape", cell_conn.shape)

    output['cell_types'] = cell_types
    output['cell_offsets'] = cell_offsets
    output['cell_conn'] = cell_conn
    return output

if __name__ == '__main__':
    read_vtu_file("pv_insitu_15674_0_0.vtu")

