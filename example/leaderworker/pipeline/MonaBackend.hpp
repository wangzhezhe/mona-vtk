/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __MONA_BACKEND_HPP
#define __MONA_BACKEND_HPP

//#include "../InSituAdaptor.hpp"
#include "Backend.hpp"
#include <mona-coll.h>
#include <mona.h>
#include <spdlog/spdlog.h>
#include <thallium.hpp>

namespace tl = thallium;

struct DataBlock
{
  // this is a generalized buffer
  std::vector<char> data;
  std::vector<size_t> dimensions;
  std::vector<int64_t> offsets;
  Type type;

  DataBlock() = default;
  DataBlock(DataBlock&&) = default;
  DataBlock(const DataBlock&) = default;
  DataBlock& operator=(DataBlock&&) = default;
  DataBlock& operator=(const DataBlock&) = default;
  ~DataBlock() = default;
};

/**
 * MonaBackend implementation of an colza Backend.
 */
class MonaBackendPipeline : public Backend
{

protected:
  tl::engine m_engine;
  std::map<uint64_t,      // iteration
    std::map<std::string, // dataset name
      std::map<uint64_t,  // block id
        DataBlock> > >
    m_datasets;
  tl::mutex m_datasets_mtx;

public:
  /**
   * @brief Constructor.
   */
  MonaBackendPipeline(const tl::engine& engine, std::string script_name, mona_instance_t mona)
    : m_engine(engine)
    , m_script_name(script_name)
  {
    if (m_mona_comm_self == nullptr)
    {
      na_addr_t self_addr = NA_ADDR_NULL;
      na_return_t ret = mona_addr_self(mona, &self_addr);
      if (ret != NA_SUCCESS)
      {
        spdlog::critical("{}: mona_addr_self returned {}", __FUNCTION__, ret);
        throw std::runtime_error("mona_addr_self failed");
      }
      ret = mona_comm_create(mona, 1, &self_addr, &m_mona_comm_self);
      if (ret != NA_SUCCESS)
      {
        spdlog::critical("{}: mona_comm_create returned {}", __FUNCTION__, ret);
        throw std::runtime_error("mona_comm_create failed");
      }
      mona_addr_free(mona, self_addr);
    }
  }

  /**
   * @brief Move-constructor.
   */
  MonaBackendPipeline(MonaBackendPipeline&&) = delete;

  /**
   * @brief Copy-constructor.
   */
  MonaBackendPipeline(const MonaBackendPipeline&) = delete;

  /**
   * @brief Move-assignment operator.
   */
  MonaBackendPipeline& operator=(MonaBackendPipeline&&) = delete;

  /**
   * @brief Copy-assignment operator.
   */
  MonaBackendPipeline& operator=(const MonaBackendPipeline&) = delete;

  /**
   * @brief Destructor.
   */
  virtual ~MonaBackendPipeline(){};

  /**
   * @brief Stage some data.
   */

  int stage(const std::string& sender_addr, const std::string& dataset_name, uint64_t iteration,
    uint64_t block_id, const std::vector<size_t>& dimensions, const std::vector<int64_t>& offsets,
    const Type& type, const thallium::bulk& data);

  /**
   * @brief The execute method in this backend is not doing anything.
   * the m_mona_comm is set by the dynamic
   */
  int execute(uint64_t iteration, mona_comm_t m_mona_comm);

  int cleanup(uint64_t iteration);

  /**
   * @brief the mona communicator associated with current pipeline
   *
   */
  // MoNA communicator with only this process, set this to make the in-staging processing work
  mona_comm_t m_mona_comm_self = nullptr;
  bool m_first_init = true;
  std::string m_script_name = "";
};

#endif
