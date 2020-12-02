// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
