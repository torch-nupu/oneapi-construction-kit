// Copyright (C) Codeplay Software Limited
//
// Licensed under the Apache License, Version 2.0 (the "License") with LLVM
// Exceptions; you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://github.com/codeplaysoftware/oneapi-construction-kit/blob/main/LICENSE.txt
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <base/context.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/LLVMContext.h>
#include <spirv-ll/context.h>
#include <spirv-ll/module.h>

#if defined(CA_BUILD_LLVM_SPIRV)
#include <LLVMSPIRVLib.h>
#include <LLVMSPIRVOpts.h>
#endif

#if !defined(NDEBUG) || defined(CA_ENABLE_LLVM_OPTIONS_IN_RELEASE)
#include <compiler/utils/llvm_global_mutex.h>
#include <llvm/Support/CommandLine.h>
#endif

namespace compiler {
BaseContext::BaseContext() {
#if !defined(NDEBUG) || defined(CA_ENABLE_LLVM_OPTIONS_IN_RELEASE)
  static std::once_flag parseEnvironmentOptionsFlag;
  std::call_once(parseEnvironmentOptionsFlag, [this]() {
    const char *argv[] = {"ComputeAortaCL"};
    const std::lock_guard<std::mutex> lock(
        compiler::utils::getLLVMGlobalMutex());
    llvm::cl::ParseCommandLineOptions(1, argv, "", nullptr, "CA_LLVM_OPTIONS");

    const llvm::StringMap<llvm::cl::Option *> &opt_map =
        llvm::cl::getRegisteredOptions();

    if (const auto *opt = opt_map.lookup("time-passes")) {
      llvm_time_passes =
          static_cast<const llvm::cl::opt<bool, true> *>(opt)->getValue();
    }

    if (const auto *opt = opt_map.lookup("verify-each")) {
      llvm_verify_each =
          static_cast<const llvm::cl::opt<bool> *>(opt)->getValue();
    }

    if (const auto *opt = opt_map.lookup("debug-pass-manager")) {
      llvm_debug_passes =
          static_cast<const llvm::cl::opt<compiler::utils::DebugLogging> *>(opt)
              ->getValue();
    }
  });
#endif
#if defined(CA_BUILD_LLVM_SPIRV)
  // TODO: make it default to use `llvm-spirv`
  const auto *env = std::getenv("CA_USE_LLVM_SPIRV");
  if (env && std::strcmp(env, "1") == 0) {
    using_llvm_spirv = true;
  }
#endif
}

BaseContext::~BaseContext() {}

bool BaseContext::isValidSPIRV(cargo::array_view<const uint32_t> code) {
  if (code.size() < 1) {
    // Need at least one word in `code` to be able to check the magic number.
    return false;
  }

  auto moduleHeader = spirv_ll::ModuleHeader{{code.data(), code.size()}};
  return moduleHeader.isValid();
}

cargo::expected<spirv::SpecializableConstantsMap, std::string>
BaseContext::getSpecializableConstants(cargo::array_view<const uint32_t> code) {
  if (using_llvm_spirv) {
#if defined(CA_BUILD_LLVM_SPIRV)
    std::istringstream iss(
        std::string(reinterpret_cast<const char *>(code.data()),
                    code.size() * sizeof(uint32_t)),
        std::ios_base::binary);
    std::vector<llvm::SpecConstInfoTy> SpecConstInfo;
    if (!llvm::getSpecConstInfo(iss, SpecConstInfo)) {
      return cargo::make_unexpected("failed in getSpecConstInfo()");
    }

    spirv::SpecializableConstantsMap constants_map;
    for (const auto &entry : SpecConstInfo) {
      auto &constant = constants_map[entry.ID];
      if (entry.Type == "i1") {
        constant.constant_type = spirv::SpecializationType::BOOL;
      } else if (entry.Type == "i32") {
        constant.constant_type = spirv::SpecializationType::INT;
      } else if (entry.Type == "f32") {
        constant.constant_type = spirv::SpecializationType::FLOAT;
      } else {
        return cargo::make_unexpected("unknown `SpecializationType`");
      }
      constant.size_in_bits = entry.Size;
    }
    return {std::move(constants_map)};
#endif
  }

  auto specializable =
      spirv_ll::Context{}.getSpecializableConstants({code.data(), code.size()});
  if (!specializable) {
    return cargo::make_unexpected(specializable.error().message);
  }

  spirv::SpecializableConstantsMap constants_map;
  for (const auto &entry : *specializable) {
    auto &constant = constants_map[entry.getFirst()];
    switch (entry.getSecond().constantType) {
      case spirv_ll::SpecializationType::BOOL:
        constant.constant_type = spirv::SpecializationType::BOOL;
        break;
      case spirv_ll::SpecializationType::INT:
        constant.constant_type = spirv::SpecializationType::INT;
        break;
      case spirv_ll::SpecializationType::FLOAT:
        constant.constant_type = spirv::SpecializationType::FLOAT;
        break;
    }
    constant.size_in_bits = entry.getSecond().sizeInBits;
  }
  return {std::move(constants_map)};
}

void BaseContext::lock() { base_context_mutex.lock(); }

bool BaseContext::try_lock() { return base_context_mutex.try_lock(); }

void BaseContext::unlock() { base_context_mutex.unlock(); }
}  // namespace compiler
