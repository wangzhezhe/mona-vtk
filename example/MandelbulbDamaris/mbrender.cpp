/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "mb.hpp"
#include <fstream>
#include <iostream>
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
        std::cout << "Found script to be " << script << std::endl;
    }

    int rank, nprocs;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &nprocs);

    auto blocks_param = damaris::ParameterManager::Search("BLOCKS");
    auto blocks_per_client = blocks_param->GetValue<int>();

    /*
    for(auto it = data->Begin(); it != data->End(); it++) {
        auto& block = *it;
        auto block_id = block->GetID();
        auto source = block->GetSource();

        auto block_id_base = blocks_per_client * source;


        int blockid = block_id_base + block_id;
        int block_offset = blockid * depth;

        // can't do that because these functions are private
        //block->SetStartIndex(2, block_offset);
        //block->SetEndIndex(2, end_index+block_offset);

        std::cout << "Rank " << rank << " has block {iteration=" << block->GetIteration()
                  << ", source=" << block->GetSource() << ", id=" << block->GetID()
                  << "} at position (" << block_offset << ","
                  << block->GetStartIndex(1) << ","
                  << block->GetStartIndex(2) << ")" << std::endl;
    }
    */
    for(auto it = position->Begin(); it != position->End(); it++) {
        auto block = *it;
        int64_t* pos = static_cast<int64_t*>(
                block->GetDataSpace().GetData());
        /*
        std::cout << "Rank " << rank << " has block {iteration=" << iteration
                  << ", source=" << block->GetSource() << ", id=" << block->GetID()
                  << "} at position (" << pos[0] << "," << pos[1] << ","
                  << pos[2] << ")" << std::endl;
        */
    }

    if(first_iteration) {
        first_iteration = false;
        InSitu::MPIInitialize(script, comm);
    }

    // cleanup data
    std::cout << "Cleaning up data" << std::endl;
    data->ClearIteration(iteration);
}
