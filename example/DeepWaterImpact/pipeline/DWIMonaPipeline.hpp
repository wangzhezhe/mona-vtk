/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __DWI_MONA_BACKEND_HPP
#define __DWI_MONA_BACKEND_HPP

#include "DataBlock.hpp"
#include <colza/Backend.hpp>
#include <thallium.hpp>
#include <mona-coll.h>
#include <mona.h>

using json = nlohmann::json;
namespace tl = thallium;

class DWIMONAPipeline : public colza::Backend
{

protected:
  tl::engine m_engine;
  ssg_group_id_t m_gid;
  json m_config;

  std::map<uint64_t,        // iteration
    std::map<uint64_t,      // block id
      std::map<std::string, // dataset name
        std::shared_ptr<DataBlock> > > >
    m_datasets;
  tl::mutex m_datasets_mtx;

public:
  /**
   * @brief Constructor.
   */
  DWIMONAPipeline(const colza::PipelineFactoryArgs& args)
    : m_engine(args.engine)
    , m_gid(args.gid)
    , m_config(args.config)
  {
  }

  /**
   * @brief Move-constructor.
   */
  DWIMONAPipeline(DWIMONAPipeline&&) = delete;

  /**
   * @brief Copy-constructor.
   */
  DWIMONAPipeline(const DWIMONAPipeline&) = delete;

  /**
   * @brief Move-assignment operator.
   */
  DWIMONAPipeline& operator=(DWIMONAPipeline&&) = delete;

  /**
   * @brief Copy-assignment operator.
   */
  DWIMONAPipeline& operator=(const DWIMONAPipeline&) = delete;

  /**
   * @brief Destructor.
   */
  virtual ~DWIMONAPipeline() = default;

  /**
   * @brief Update the array of Mona addresses associated with
   * the SSG group.
   *
   * @param mona Mona instance.
   * @param addresses Array of Mona addresses.
   */
  void updateMonaAddresses(mona_instance_t mona, const std::vector<na_addr_t>& addresses) override;

  /**
   * @brief Tells the pipeline that the given iteration is starting.
   * This function should be called before stage/execute/cleanup can
   * be called.
   *
   * @param iteration Iteration
   *
   * @return a RequestResult containing an error code.
   */
  colza::RequestResult<int32_t> start(uint64_t iteration) override;

  /**
   * @brief Tells the pipeline that the given iteration is aborted.
   * This function is used automatically when there is a mismatch
   * between the client's view of the group and the group itself.
   *
   * @param iteration Iteration
   */
  void abort(uint64_t iteration) override;

  /**
   * @brief Stage some data.
   */
  colza::RequestResult<int32_t> stage(const std::string& sender_addr,
    const std::string& dataset_name, uint64_t iteration, uint64_t block_id,
    const std::vector<size_t>& dimensions, const std::vector<int64_t>& offsets,
    const colza::Type& type, const thallium::bulk& data) override;

  /**
   * @brief The execute method in this backend is not doing anything.
   */
  colza::RequestResult<int32_t> execute(uint64_t iteration) override;

  /**
   * @brief Erase all the data blocks associated with a given iteration.
   */
  colza::RequestResult<int32_t> cleanup(uint64_t iteration) override;

  /**
   * @brief Destroys the underlying pipeline.
   *
   * @return a RequestResult<int32_t> instance indicating
   * whether the database was successfully destroyed.
   */
  colza::RequestResult<int32_t> destroy() override;

  /**
   * @brief Static factory function used by the PipelineFactory to
   * create a DWIMONAPipeline.
   *
   * @param args arguments used for creating the pipeline.
   *
   * @return a unique_ptr to a pipeline
   */
  static std::unique_ptr<colza::Backend> create(const colza::PipelineFactoryArgs& args);


  /**
   * @brief the mona communicator associated with current pipeline
   *
   */
  mona_instance_t        m_mona = nullptr;
  mona_comm_t            m_mona_comm = nullptr; // MoNA communicator built in start()
  mona_comm_t            m_mona_comm_self = nullptr; // MoNA communicator with only this process
  std::vector<na_addr_t> m_member_addrs; // latest known member addresses

  // do not update comm when it is used by the in-situ part
  tl::mutex m_mona_comm_mtx;

  int m_init_rank = 0;
  int m_init_proc = 0;
  bool m_first_init = true;
  std::string m_script_name = "";


};

#endif
