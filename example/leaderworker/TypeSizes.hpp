/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __TYPE_SIZES_H
#define __TYPE_SIZES_H

enum class Type : uint32_t
{
  INT8,
  UINT8,
  INT16,
  UINT16,
  INT32,
  UINT32,
  INT64,
  UINT64,
  FLOAT32,
  FLOAT64
};

inline size_t ComputeDataSize(const std::vector<size_t>& dimensions, const Type& type)
{
  size_t s = 1;
  for (auto d : dimensions)
    s *= d;
  switch (type)
  {
    case Type::INT8:
    case Type::UINT8:
      return s;
    case Type::INT16:
    case Type::UINT16:
      return 2 * s;
    case Type::FLOAT32:
    case Type::INT32:
    case Type::UINT32:
      return 4 * s;
    case Type::INT64:
    case Type::UINT64:
    case Type::FLOAT64:
      return 8 * s;
  }
  return 0;
}

#endif
