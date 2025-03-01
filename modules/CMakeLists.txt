# Copyright (C) Codeplay Software Limited
#
# Licensed under the Apache License, Version 2.0 (the "License") with LLVM
# Exceptions; you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://github.com/codeplaysoftware/oneapi-construction-kit/blob/main/LICENSE.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cargo)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/loader)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tracer)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/debug)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/utils)

# Fetch the official SPIR-V headers. These are required by some up our public
# compiler headers, which are also included by mux, so this must take place
# before including both mux and compiler modules.
# We skip the FetchContent if we are building in LLVM's tree and llvm-spirv
# is among the enabled projects, since this FetchContent call conflicts with
# the one in llvm-spirv.
if(NOT (OCK_IN_LLVM_TREE AND "llvm-spirv" IN_LIST LLVM_ENABLE_PROJECTS))
  include(FetchContent)
  FetchContent_Declare(
    SPIRV-Headers
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
    GIT_TAG        3f17b2af6784bfa2c5aa5dbb8e0e74a607dd8b3b
  )
  FetchContent_MakeAvailable(SPIRV-Headers)

  if(NOT spirv-headers_POPULATED)
    message(FATAL_ERROR "No SPIRV-Headers found.")
  endif()

  if(CA_BUILD_SPIRV_TOOLS)
    FetchContent_Declare(
      SPIRV-Tools
      GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
      GIT_TAG        vulkan-sdk-1.3.275.0
      GIT_SHALLOW 1
    )
    set(SPIRV_SKIP_TESTS ON)
    set(SPIRV_WERROR OFF)
    FetchContent_MakeAvailable(SPIRV-Tools)
  endif()
endif()

if(CA_BUILD_LLVM_SPIRV)
  # TODO: latest llvm-spirv requires llvm 20.x
  set(LLVM_EXTERNAL_SPIRV_HEADERS_SOURCE_DIR "${spirv-headers_SOURCE_DIR}")
  set(LLVM_SPIRV_BUILD_EXTERNAL ON)
  set(LLVM_SPIRV_INCLUDE_TESTS OFF)
  add_subdirectory(${CMAKE_SOURCE_DIR}/llvm-spirv ${CMAKE_CURRENT_BINARY_DIR}/llvm-spirv)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/mux)
# compiler/builtins requires capabilities from ComputeMux targets.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compiler)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/metadata)
if(CA_ENABLE_TESTS)
  # mux tests depend on builtins.
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/mux/test)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/kts)
endif()
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lit)
