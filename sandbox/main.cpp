#include <nil/sml.hpp>

#include <cstdio>
#include <iostream>

struct e1
{
};

struct e2
{
};

struct e3
{
};

struct sub2;

struct sub1 // NOLINT
{
    using events = nil::xalt::tlist<e1>;

    static auto on_event(const e1& /* ev */)
    {
        std::cout << "sub1 - e1" << std::endl;
        return nil::sml::Transit<sub2>();
    }
};

struct sub2 // NOLINT
{
    using events = nil::xalt::tlist<e1>;

    static auto on_event(const e1& /* ev */)
    {
        std::cout << "sub2 - e1" << std::endl;
        return nil::sml::Forward{};
    }
};

struct composite // NOLINT
{
    using regions = nil::xalt::tlist<sub1>;

    using events = nil::xalt::tlist<e1>;

    static auto on_event(const e1& /* ev */)
    {
        std::cout << "composite - e1" << std::endl;
        return nil::sml::Discard{};
    }

    static auto on_event(const e2& /* ev */) -> std::variant<nil::sml::Discard>
    {
        std::cout << "composite - e2" << std::endl;
        return nil::sml::Discard{};
    }
};

int main()
{
    nil::sml::SM<nil::xalt::tlist<composite>> ss{};

    {
        e1 e;
        ss.process_event(e);
    }
    {
        e1 e;
        ss.process_event(e);
    }
    {
        e2 e;
        ss.process_event(e);
    }
}
