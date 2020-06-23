#pragma once

#include <pcl/experimental/execution/executor/base_executor.hpp>
#include <pcl/experimental/execution/executor/cuda_executor.hpp>
#include <pcl/experimental/execution/executor/inline_executor.hpp>
#include <pcl/experimental/execution/executor/omp_executor.hpp>
#include <pcl/experimental/execution/executor/sse_executor.hpp>

using best_fit = inline_executor<oneway_t, single_t, blocking_t::always_t>;
