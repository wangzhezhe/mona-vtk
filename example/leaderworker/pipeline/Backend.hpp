/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __LEADERWORKER_BACKEND_HPP
#define __LEADERWORKER_BACKEND_HPP

#include <functional>
#include <mona-coll.h>
#include <mona.h>
#include <nlohmann/json.hpp>
#include <thallium.hpp>
#include <unordered_map>
#include <unordered_set>

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

/**
 * @brief Interface for pipeline backends. To build a new backend,
 * implement a class MyBackend that inherits from Backend, and put
 * COLZA_REGISTER_BACKEND(mybackend, MyBackend); in a cpp file
 * that includes your backend class' header file.
 *
 * Your backend class should also have a static function to
 * create a pipeline:
 *
 * std::unique_ptr<Backend> create(ssg_group_id_t gid, const json& config)
 */
class Backend
{

public:
  /**
   * @brief Constructor.
   */
  Backend() = default;

  /**
   * @brief Move-constructor.
   */
  Backend(Backend&&) = default;

  /**
   * @brief Copy-constructor.
   */
  Backend(const Backend&) = default;

  /**
   * @brief Move-assignment operator.
   */
  Backend& operator=(Backend&&) = default;

  /**
   * @brief Copy-assignment operator.
   */
  Backend& operator=(const Backend&) = default;

  /**
   * @brief Destructor.
   */
  virtual ~Backend(){};

  /**
   * @brief Stage some data for a future execution of the pipeline.
   *
   * @param sender_addr Sender address
   * @param dataset_name Dataset name
   * @param iteration Iteration
   * @param block_id Block id
   * @param dimensions Dimensions
   * @param offsets Offsets along each dimension
   * @param type Type of data
   * @param data Data
   *
   * @return a RequestResult containing an error code.
   */
  virtual int stage(const std::string& sender_addr, const std::string& dataset_name,
    uint64_t iteration, uint64_t block_id, const std::vector<size_t>& dimensions,
    const std::vector<int64_t>& offsets, const Type& type, const thallium::bulk& data) = 0;

  /**
   * @brief Execute the pipeline on a specific iteration of data.
   *
   * @param iteration Iteration
   *
   * @return a RequestResult containing an error code.
   */
  virtual int execute(uint64_t& iteration, std::string& dataset_name, mona_comm_t m_mona_comm) = 0;
  
  // TODO, update this part, add another parameter to label the versio of execution function
  virtual int executesynthetic(
    uint64_t& iteration, std::string& dataset_name, mona_comm_t m_mona_comm) = 0;

  virtual int executesynthetic2(
    uint64_t& iteration, std::string& dataset_name, mona_comm_t m_mona_comm) = 0;

  virtual int cleanup(std::string& dataset_name, uint64_t iteration) = 0;
};

#endif
