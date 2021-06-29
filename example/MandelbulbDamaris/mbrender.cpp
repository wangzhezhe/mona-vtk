/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "mb.hpp"
#include <fstream>
#include <iostream>
#include <exception>
#include <mpi.h>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <tclap/CmdLine.h>
#include <Damaris.h>
#include <damaris/data/VariableManager.hpp>
#include <damaris/env/Environment.hpp>
#include <damaris/data/ParameterManager.hpp>
#include "MPIInSituAdaptor.hpp"

extern "C" void mandelbulb_render(const char* name, int32_t source, int32_t iteration, const char* args) {
    static std::shared_ptr<damaris::Variable> data;
    static std::shared_ptr<damaris::Variable> position;
    static MPI_Comm comm = MPI_COMM_NULL;
    static bool first_iteration = true;
    static std::string script;

    if(!data) {
        data = damaris::VariableManager::Search("mandelbulb");
    }
    if(!data) {
        std::cerr << "Could not find mandelbulb variable" << std::endl;
        return;
    }
    if(!position) {
        position = damaris::VariableManager::Search("position");
    }
    if(!position) {
        std::cerr << "Could not find position variable" << std::endl;
        return;
    }
    if(comm == MPI_COMM_NULL)
        comm = damaris::Environment::GetEntityComm();

    if(script.empty()) {
        auto script_name_var = damaris::VariableManager::Search("script");
        if(!script_name_var) {
            std::cerr << "Could not find script name variable" << std::endl;
            return;
        }
        auto block = script_name_var->GetBlock(source, iteration, 0);
        if(!block) {
            std::cerr << "Could not find block for script name variable" << std::endl;
        }
        auto& ds = block->GetDataSpace();
        script = static_cast<char*>(ds.GetData());
    }

    if(first_iteration) {
        first_iteration = false;
        InSitu::Initialize(script, comm);
    }

    auto blocks_param = damaris::ParameterManager::Search("BLOCKS");
    auto blocks_per_client = blocks_param->GetValue<int>();
    auto num_clients = damaris::Environment::CountTotalClients();
    auto total_blocks = num_clients*blocks_per_client;

    auto depth = damaris::ParameterManager::Search("DEPTH")->GetValue<int>();
    auto width = damaris::ParameterManager::Search("WIDTH")->GetValue<int>();
    auto height = damaris::ParameterManager::Search("HEIGHT")->GetValue<int>();

    std::vector<Mandelbulb> mandelbulbList;

    for(auto it = position->Begin(); it != position->End(); it++) {
        auto block = *it;
        int64_t* pos = static_cast<int64_t*>(
                block->GetDataSpace().GetData());
        auto block_source = block->GetSource();
        auto block_id = block->GetID();
        auto data_block = data->GetBlock(block_source, iteration, block_id);
        if(!data_block) {
            std::cerr << "ERROR: data block not found for source "
                      << source << " and block id " << block_id
                      << std::endl;
            throw std::runtime_error("data block not found");
        }
        auto global_block_id = block_source * blocks_per_client + block_id;
        auto block_offset = global_block_id * depth;
        mandelbulbList.emplace_back(
                width, height, depth, block_offset,
                1.2, total_blocks);
        auto& mb = mandelbulbList[mandelbulbList.size()-1];
        auto bytes = mb.DataSize() * sizeof(int);
        memcpy(mb.GetData(), data_block->GetDataSpace().GetData(), bytes);
    }

    InSitu::CoProcess(mandelbulbList, total_blocks, iteration, iteration);

    // cleanup data
    data->ClearIteration(iteration);
}
