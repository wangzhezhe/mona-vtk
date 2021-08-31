/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __DWI_DATABLOCK_HPP
#define __DWI_DATABLOCK_HPP


#include <vector>
#include <colza/Backend.hpp>
#include <memory>

// Add necessary data arrays
// these arrays are subcomponents to compose the vtk unstructured data
/*
struct DataBlock
{
  // this is a specilised data block for dwi example
  
   //['rho'] double
   //['v02'] double
   //['points'] int

   //['cell_types'] byte
   //['cell_offsets'] int
   //['cell_conn'] int
  
  std::vector<double> rho;
  std::vector<double> v02;
  std::vector<int> points;

  std::vector<char> cell_types;
  std::vector<int> cell_offsets;
  std::vector<int> cell_conn;

  DataBlock() = default;
  DataBlock(DataBlock&&) = default;
  DataBlock(const DataBlock&) = default;
  DataBlock& operator=(DataBlock&&) = default;
  DataBlock& operator=(const DataBlock&) = default;
  ~DataBlock() = default;
};
*/

// the generalized DataBlock for storing all kinds of array
struct DataBlock
{
  //this is a generalized buffer 
  std::vector<char> data;
  std::vector<size_t> dimensions;
  std::vector<int64_t> offsets;
  colza::Type type;

  DataBlock() = default;
  DataBlock(DataBlock&&) = default;
  DataBlock(const DataBlock&) = default;
  DataBlock& operator=(DataBlock&&) = default;
  DataBlock& operator=(const DataBlock&) = default;
  ~DataBlock() = default;
};

#endif