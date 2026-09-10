// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

namespace toll::ev
{
    struct ok
    {
    };

    struct fail
    {
    };

    struct tick
    {
    };

    struct arrive
    {
        int vehicle_class = 1;
    };

    struct pay
    {
        int amount = 0;
    };

    struct receipt
    {
    };

    struct cancel
    {
    };

    struct depart
    {
    };

    struct gate_open
    {
    };

    struct gate_close
    {
    };

    struct alarm
    {
    };

    struct reset
    {
    };

    struct abort
    {
    };

    struct shutdown
    {
    };

    struct select_startup
    {
    };

    struct select_collection
    {
    };

    struct select_shift
    {
    };

    struct select_maintenance
    {
    };
}
