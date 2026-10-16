#pragma once

#include <string_view>

// Generated from the files in `assets` at build time, see embed.cmake.
namespace nil::sm::viz::assets
{
    std::string_view index_html();
    std::string_view index_js();
}
