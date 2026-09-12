// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "../tollbooth/common.hpp"
#include <nil/sm/barrier.hpp>

namespace toll::bslot
{
    NIL_SM_BARRIER_DECLARE(collection_barrier_state, nil::sm::api::Coalesce<tracing_api>);
}
