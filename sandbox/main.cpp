#include <nil/sm.hpp>

#include <cstdio>
#include <iostream>

namespace demo::events
{
    struct e1
    {
    };

    struct e2
    {
    };

    struct e3
    {
    };
}

namespace demo::states
{
    template <int N, typename FlavorTag>
    struct sub2;

    template <int N, typename FlavorTag>
    struct sub1 // NOLINT
    {
        using events = nil::xalt::tlist<demo::events::e1>;

        static auto on_event(const demo::events::e1& /* ev */)
        {
            std::cout << "sub1<" << N << "> - e1" << std::endl;
            return nil::sm::Transit<sub2<N + 1, FlavorTag>>();
        }
    };

    template <int N, typename FlavorTag>
    struct sub2 // NOLINT
    {
        using events = nil::xalt::tlist<demo::events::e1>;

        static auto on_event(const demo::events::e1& /* ev */)
        {
            std::cout << "sub2<" << N << "> - e1" << std::endl;
            return nil::sm::Terminate{};
        }
    };

    template <typename FlavorTag, int CompositeId>
    struct composite // NOLINT
    {
        using regions = nil::xalt::tlist<sub1<CompositeId, FlavorTag>>;

        using events = nil::xalt::tlist<demo::events::e1>;

        static auto on_event(const demo::events::e1& /* ev */)
        {
            std::cout << "composite<" << CompositeId << "> - e1" << std::endl;
            return nil::sm::Discard{};
        }

        static auto on_event(const demo::events::e2& /* ev */) -> std::variant<nil::sm::Discard>
        {
            std::cout << "composite<" << CompositeId << "> - e2" << std::endl;
            return nil::sm::Discard{};
        }

        static auto on_regions_finalized()
        {
            std::cout << "composite<" << CompositeId << "> - regions finalized" << std::endl;
            return nil::sm::Terminate{};
        }
    };

    template <typename FlavorTag>
    struct multi_region // NOLINT
    {
        using regions = nil::xalt::tlist<composite<FlavorTag, 7>, composite<FlavorTag, 11>>;

        static auto on_regions_finalized()
        {
            return nil::sm::Terminate{};
        }
    };
}

struct SandboxAPIContext
{
};

template <typename T>
struct SandboxAPI
{
    using api_context_t = SandboxAPIContext;

    static void print(std::ostream& out, const nil::sm::state_metadata* metadata)
    {
        if (metadata == nullptr)
        {
            return;
        }

        if (metadata->parent == nullptr)
        {
            out << metadata->name;
            return;
        }

        print(out, metadata->parent);

        out << " --> ";
        out << metadata->name;
    }

    template <typename Parent>
    static T make(
        Parent* parent,
        void* state_contexts,
        SandboxAPIContext* /* api_contexts */,
        const nil::sm::state_metadata& metadata
    )
    {
        auto r = nil::sm::default_api<T>::make(parent, state_contexts, nullptr, metadata);

        if (metadata.region_count == 0)
        {
            print(std::cout, &metadata);
            std::cout << std::endl;
        }

        return r;
    }

    static auto on_enter(T& state, SandboxAPIContext* /* api_contexts */)
    {
        return nil::sm::default_api<T>::on_enter(state, nullptr);
    }
};

int main()
{
    struct random_flavor
    {
    };

    using top_state = demo::states::multi_region<random_flavor>;

    SandboxAPIContext api_context;
    nil::sm::SM<nil::sm::coalesce_api<SandboxAPI>::type, top_state> ss{nullptr, &api_context};

    {
        demo::events::e1 e;
        ss.post(e);
    }
    {
        demo::events::e1 e;
        ss.post(e);
    }
    {
        demo::events::e2 e;
        ss.post(e);
    }
}
