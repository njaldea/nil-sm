#include "nil/sm/formatter/xstate.hpp"
#include <nil/sm/uml.hpp>

#include <iostream>

namespace sandbox::uml_example
{
    struct ev_submit_order
    {
    };

    struct ev_validation_ok
    {
    };

    struct ev_validation_failed
    {
    };

    struct ev_payment_ok
    {
    };

    struct ev_payment_failed
    {
    };

    struct ev_pick_done
    {
    };

    struct ev_pack_done
    {
    };

    struct ev_ship
    {
    };

    struct ev_delivered
    {
    };

    struct ev_notify_customer
    {
    };

    struct ev_notification_sent
    {
    };

    struct ev_status_ping
    {
    };

    struct ev_escalate
    {
    };

    struct ev_cancel
    {
    };

    struct draft_order;
    struct validating_order;
    struct awaiting_payment;
    struct paid_order;

    struct draft_order
    {
        using events = nil::xalt::tlist<ev_submit_order, ev_cancel>;

        static auto on_enter()
        {
            return nil::sm::NOOP();
        }

        static auto on_event(const ev_submit_order& /* event */)
        {
            return nil::sm::Transit<validating_order>{};
        }

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct validating_order
    {
        using events = nil::xalt::tlist<ev_validation_ok, ev_validation_failed, ev_cancel>;

        static auto on_event(const ev_validation_ok& /* event */)
        {
            return nil::sm::Transit<awaiting_payment>{};
        }

        static auto on_event(const ev_validation_failed& /* event */)
        {
            return nil::sm::Terminate{};
        }

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct awaiting_payment
    {
        using events = nil::xalt::tlist<ev_payment_ok, ev_payment_failed, ev_cancel>;

        static auto on_event(const ev_payment_ok& /* event */)
        {
            return nil::sm::Transit<paid_order>{};
        }

        static auto on_event(const ev_payment_failed& /* event */)
        {
            return nil::sm::Terminate{};
        }

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct paid_order
    {
        using events = nil::xalt::tlist<ev_cancel>;

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct order_lifecycle
    {
        using regions = nil::xalt::tlist<draft_order>;
        using events = nil::xalt::tlist<ev_cancel>;

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct picking;
    struct packing;
    struct ready_to_ship;

    struct picking
    {
        using events = nil::xalt::tlist<ev_pick_done, ev_cancel>;

        static auto on_event(const ev_pick_done& /* event */)
        {
            return nil::sm::Transit<packing>{};
        }

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct packing
    {
        using events = nil::xalt::tlist<ev_pack_done, ev_cancel>;

        static auto on_event(const ev_pack_done& /* event */)
        {
            return nil::sm::Transit<ready_to_ship>{};
        }

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct ready_to_ship
    {
        using events = nil::xalt::tlist<ev_ship, ev_status_ping, ev_cancel>;

        static auto on_event(const ev_ship& /* event */)
        {
            return nil::sm::Terminate{};
        }

        static auto on_event(const ev_status_ping& /* event */)
        {
            return nil::sm::Discard{};
        }

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct waiting_for_courier;
    struct in_transit;
    struct delivered;

    struct waiting_for_courier
    {
        using events = nil::xalt::tlist<ev_ship, ev_cancel>;

        static auto on_event(const ev_ship& /* event */)
        {
            return nil::sm::Transit<in_transit>{};
        }

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct in_transit
    {
        using events = nil::xalt::tlist<ev_delivered, ev_cancel>;

        static auto on_event(const ev_delivered& /* event */)
        {
            return nil::sm::Transit<delivered>{};
        }

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct delivered
    {
        using events = nil::xalt::tlist<ev_cancel>;

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct fulfillment_lifecycle
    {
        using regions = nil::xalt::tlist<picking, waiting_for_courier>;
        using events = nil::xalt::tlist<ev_cancel>;

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct notify_pending;
    struct notify_sent;

    struct notify_pending
    {
        using events = nil::xalt::tlist<ev_notify_customer, ev_escalate, ev_cancel>;

        static auto on_event(const ev_notify_customer& /* event */)
        {
            return nil::sm::Transit<notify_sent>{};
        }

        static auto on_event(const ev_escalate& /* event */)
        {
            return nil::sm::Forward{};
        }

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct notify_sent
    {
        using events = nil::xalt::tlist<ev_notification_sent, ev_cancel>;

        static auto on_event(const ev_notification_sent& /* event */)
        {
            return nil::sm::Terminate{};
        }

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct notifications
    {
        using regions = nil::xalt::tlist<notify_pending>;
        using events = nil::xalt::tlist<ev_escalate, ev_cancel>;

        static auto on_event(const ev_escalate& /* event */)
        {
            return nil::sm::Discard{};
        }

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct order_workflow
    {
        // Three orthogonal lanes running in parallel.
        using regions = nil::xalt::tlist<order_lifecycle, fulfillment_lifecycle, notifications>;
        using events = nil::xalt::tlist<ev_cancel>;

        static auto on_event(const ev_cancel& /* event */)
        {
            return nil::sm::Terminate{};
        }

        static auto on_regions_finalized()
        {
            return nil::sm::Terminate{};
        }
    };
}

int main(int argc, const char** argv)
{
    using SM = nil::sm::DefaultSM<sandbox::uml_example::order_workflow>;
    if (argc > 1)
    {
        std::string_view t = argv[1];
        if (t == "puml")
        {
            std::cout << nil::sm::puml<SM>();
            return 0;
        }
        if (t == "mermaid")
        {
            std::cout << nil::sm::mermaid<SM>();
            return 0;
        }
        if (t == "dot")
        {
            std::cout << nil::sm::dot<SM>();
            return 0;
        }
        if (t == "scxml")
        {
            std::cout << nil::sm::scxml<SM>();
            return 0;
        }
        if (t == "xstate")
        {
            std::cout << nil::sm::xstate<SM>();
            return 0;
        }
    }

    std::cout << "- puml\n"
                 "- mermaid\n"
                 "- dot\n"
                 "- scxml\n"
                 "- xstate\n"
              << std::flush;
    return 1;
}
