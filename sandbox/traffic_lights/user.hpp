#pragma once

#include <iostream>

#include <nil/sm.hpp>
#include <nil/sm/format/puml.hpp>

struct ev_tick
{
};

struct st_red;
struct st_green;
struct st_yellow;

struct st_red
{
    using events = nil::xalt::tlist<ev_tick>;

    static auto on_event(const ev_tick& /* event */)
    {
        std::cout << "tick: red[1]\n";
        std::cout << "red -> green\n";
        return nil::sm::TransitTo<st_green>{};
    }
};

struct st_green
{
    using events = nil::xalt::tlist<ev_tick>;

    auto on_event(const ev_tick& /* event */)
        -> std::variant<nil::sm::Discard, nil::sm::TransitTo<st_yellow>>
    {
        ticks++;
        std::cout << "tick: green[" << ticks << "]\n";

        if (ticks < 5)
        {
            return nil::sm::Discard{};
        }

        std::cout << "green -> yellow\n";
        return nil::sm::TransitTo<st_yellow>{};
    }

private:
    int ticks = 0;
};

struct st_yellow
{
    auto on_event(const ev_tick& /* event */)
        -> std::variant<nil::sm::Discard, nil::sm::TransitTo<st_red>>
    {
        ticks++;
        std::cout << "tick: yellow[" << ticks << "]\n";

        if (ticks < 3)
        {
            return nil::sm::Discard{};
        }

        std::cout << "yellow -> red\n";
        return nil::sm::TransitTo<st_red>{};
    }

private:
    int ticks = 0;
};

struct st_main
{
    using regions = nil::xalt::tlist<st_red>;

    using events = nil::xalt::tlist<ev_tick>;

    static auto on_event(const ev_tick& /* event */)
    {
        std::cout << "main: unhandled ev_tick\n";
        return nil::sm::Discard{};
    }
};

// https://godbolt.org/z/PMKsdP31P
