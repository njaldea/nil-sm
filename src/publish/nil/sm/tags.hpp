#pragma once

#include <nil/xalt/coalesce.hpp>
#include <nil/xalt/tlist.hpp>

namespace nil::sm::detail
{
    NIL_XALT_COALESCE_TAG(regions, nil::xalt::tlist<>);
    NIL_XALT_COALESCE_TAG(events, nil::xalt::tlist<>);
    NIL_XALT_COALESCE_TAG(captures, nil::xalt::tlist<>);
    NIL_XALT_COALESCE_TAG(args, nil::xalt::tlist<>);
    NIL_XALT_COALESCE_TAG(provides, nil::xalt::tlist<>);
}
