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

extern "C" void mandelbulb_render(const char* name, int32_t source, int32_t iteration, const char* args) {
    std::cerr << "Called signal " << name << " for source " << source << " iteration " << iteration << std::endl;
    auto var = damaris::VariableManager::Search("mandelbulb");
    if(var) {
        std::cerr << "mandelbulb variable correctly found" << std::endl;
    }
}
