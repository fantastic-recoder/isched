// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_CustomNode.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Translation unit for CustomNode — keeps the vtable anchored here.
 * @author isched Development Team
 * @version 1.0.0
 * @date 2026-04-06
 */

#include <isched/shared/ast/isched_CustomNode.hpp>

// CustomNode is a struct with only inline members.
// This translation unit exists to anchor the vtable (inherited from
// tao::pegtl::parse_tree::node) to a single object file, satisfying
// the C++ Core Guideline that each polymorphic class has a TU.
