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
#include "MPIInSituAdaptor.hpp"

extern "C" void mandelbulb_render(const char* name, int32_t source, int32_t iteration, const char* args) {
    static std::shared_ptr<damaris::Variable> var;
    static MPI_Comm comm = MPI_COMM_NULL;
    static bool first_iteration = true;
    static std::string script;

    if(!var) {
        var = damaris::VariableManager::Search("mandelbulb");
    }
    if(!var) {
        std::cerr << "Could not find mandelbulb variable" << std::endl;
        return;
    } else {
        std::cout << "Correctly found mandelbulb variable" << std::endl;
    }
    if(comm == MPI_COMM_NULL) comm = damaris::Environment::GetEntityComm();

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

    if(first_iteration) {
        first_iteration = false;
    //    InSitu::MPIInitialize(script, comm);
    }
}
