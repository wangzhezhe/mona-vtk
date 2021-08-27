/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "DWIPipeline.hpp"
#include <spdlog/spdlog.h>

COLZA_REGISTER_BACKEND(dwi_pipeline, DWIPipeline);

void DWIPipeline::updateMonaAddresses(
        mona_instance_t mona,
        const std::vector<na_addr_t>& addresses) {
    spdlog::trace("Mona addresses have been updated, group size is now {}", addresses.size());
    (void)addresses;
    (void)mona;
    // TODO
}

colza::RequestResult<int32_t> DWIPipeline::start(uint64_t iteration) {
    spdlog::trace("Iteration {} starting", iteration);
    colza::RequestResult<int32_t> result;
    result.success() = true;
    result.value() = 0;
    // TODO
    return result;
}

void DWIPipeline::abort(uint64_t iteration) {
    spdlog::trace("Iteration {} aborted", iteration);
}

colza::RequestResult<int32_t> DWIPipeline::execute(
        uint64_t iteration) {
    (void)iteration;
    spdlog::trace("Iteration {} executing", iteration);
    // TODO
    auto result = colza::RequestResult<int32_t>();
    result.value() = 0;
    return result;
}

colza::RequestResult<int32_t> DWIPipeline::cleanup(
        uint64_t iteration) {
    spdlog::trace("Iteration {} cleaned up", iteration);
    auto result = colza::RequestResult<int32_t>();
    result.value() = 0;
    // TODO
    return result;
}

colza::RequestResult<int32_t> DWIPipeline::stage(
        const std::string& sender_addr,
        const std::string& dataset_name,
        uint64_t iteration,
        uint64_t block_id,
        const std::vector<size_t>& dimensions,
        const std::vector<int64_t>& offsets,
        const colza::Type& type,
        const thallium::bulk& data) {
    colza::RequestResult<int32_t> result;
    spdlog::trace("Block {} for dataset {} sent by {} at iteration {}",
            block_id, dataset_name, sender_addr, iteration);
    // TODO
    result.value() = 0;
    return result;
}

colza::RequestResult<int32_t> DWIPipeline::destroy() {
    colza::RequestResult<int32_t> result;
    result.value() = true;
    // TODO
    return result;
}

std::unique_ptr<colza::Backend> DWIPipeline::create(const colza::PipelineFactoryArgs& args) {
    return std::unique_ptr<colza::Backend>(new DWIPipeline(args));
}
