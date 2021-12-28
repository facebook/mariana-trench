/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/GroupHashedSetAbstractDomain.h>
#include <mariana-trench/Issue.h>

namespace marianatrench {

/**
 * Represents an abstract set of issues.
 */
using IssueSet =
    GroupHashedSetAbstractDomain<Issue, Issue::GroupHash, Issue::GroupEqual>;

} // namespace marianatrench
