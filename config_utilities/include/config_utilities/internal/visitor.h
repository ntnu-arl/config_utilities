/** -----------------------------------------------------------------------------
 * Copyright (c) 2023 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * AUTHORS:     Lukas Schmid <lschmid@mit.edu>, Nathan Hughes <na26933@mit.edu>
 * AFFILIATION: MIT-SPARK Lab, Massachusetts Institute of Technology
 * YEAR:        2023
 * LICENSE:     BSD 3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * -------------------------------------------------------------------------- */

#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "config_utilities/internal/meta_data.h"
#include "config_utilities/internal/namespacing.h"
#include "config_utilities/internal/yaml_parser.h"

namespace config::internal {

/**
 * @brief The visitor gets and sets information between the meta-data and configs. It is hidden via in-thread singleton
 * sequential access to make the public user interface cleaner. This is thread safe and creates one visitor instance per
 * thread, so it can be used as if single-threaded.
 */
struct Visitor {
  ~Visitor();
  static bool hasInstance();

  // Interfaces for all internal tools interact with configs through the visitor.
  // Set the data in the config from the node.
  template <typename ConfigT>
  static MetaData setValues(ConfigT& config,
                            const YAML::Node& node,
                            const bool print_warnings = true,
                            const std::string& name_space = "",
                            const std::string& field_name = "");

  // Get the data stored in the config.
  template <typename ConfigT>
  static MetaData getValues(const ConfigT& config,
                            const bool print_warnings = true,
                            const std::string& name_space = "",
                            const std::string& field_name = "");

  // Execute all checks specified in the config.
  template <typename ConfigT>
  static MetaData getChecks(const ConfigT& config, const std::string& field_name = "");

  /** Interfaces for the config declaration interfaces to to expose their info to the visitor. **/
  // Set the name.
  static void visitName(const std::string& name);

  // Non-config types.
  template <typename T, typename std::enable_if<!isConfig<T>(), bool>::type = true>
  static void visitField(T& field, const std::string& field_name, const std::string& unit);

  // Non-config types with a conversion.
  template <typename Conversion, typename T, typename std::enable_if<!isConfig<T>(), bool>::type = true>
  static void visitField(T& field, const std::string& field_name, const std::string& unit);

  // Single config types.
  template <typename ConfigT, typename std::enable_if<isConfig<ConfigT>(), bool>::type = true>
  static void visitField(ConfigT& config, const std::string& field_name, const std::string& name_space);

  // Vector of config types.
  template <typename ConfigT, typename std::enable_if<isConfig<ConfigT>(), bool>::type = true>
  static void visitField(std::vector<ConfigT>& config, const std::string& field_name, const std::string& /* unit */);

  // Execute a check.
  static void visitCheck(const CheckBase& check);

  // Inheritance base definition.
  template <typename ConfigT>
  static void visitBase(ConfigT& config);

  // Virtual config.
  static std::optional<YAML::Node> visitVirtualConfig(bool is_set, bool is_optional, const std::string& type);

 private:
  friend class config::NameSpace;
  friend void config::enter_namespace(const std::string& name);
  friend void config::exit_namespace();
  friend void config::clear_namespaces();
  friend std::string config::current_namespace();

  // Which operations to perform on the data.
  enum class Mode { kGet, kGetDefaults, kSet, kCheck };
  const Mode mode;

  // Create and access the meta data for the current thread. Lifetime of the meta data is managed internally by the
  // objects. Note that meta data always needs to be created before it can be accessed. In short, 'instance()' is only
  // to be used within the 'declare_config()' function, whereas 'create()' is to be used to extract data from a struct
  // by calling 'declare_config()'.
  explicit Visitor(Mode _mode, const std::string& _name_space = "", const std::string& _field_name = "");

  static Visitor& instance();

  /* Utility function to manipulate data. */
  // Helper function to get the default values of a config.
  template <typename ConfigT, typename std::enable_if<!is_virtual_config<ConfigT>::value, bool>::type = true>
  static MetaData getDefaults(const ConfigT& config);
  template <typename ConfigT, typename std::enable_if<is_virtual_config<ConfigT>::value, bool>::type = true>
  static MetaData getDefaults(const ConfigT& config);

  // Labels all fields in the data as default if they match the default values of the config.
  template <typename ConfigT>
  static void flagDefaultValues(const ConfigT& config, MetaData& data);

  // Extend the current visitor with a sub-visitor, replicating the previous specification.
  template <typename ConfigT>
  static MetaData subVisit(ConfigT& config,
                           const bool print_warnings,
                           const std::string& field_name,
                           const std::string& sub_ns);

  /* Internal data to handle visits. */
  // The messenger data to read from and return eventually.
  MetaData data;

  // Storage for user specified namespaces. Managed by namespacing.h.
  OpenNameSpace::Stack open_namespaces;

  // The current namespace used to get or set values.
  std::string name_space;

  // Keep track of which base configs were already visited to avoid duplicates in diamond inheritance.
  std::set<std::string> visited_base_configs;

  /* Member variables. */
  // Static registration to get access to the correct instance. Instancs are managed per thread and as a stack, i.e.
  // nested calls are possible and will always use the latest visitor.
  static thread_local std::vector<Visitor*> instances;
};

}  // namespace config::internal

#include "config_utilities/internal/visitor_impl.hpp"
