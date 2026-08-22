#pragma once

#include "structs.hpp"

#include <cstdint>
#include <string_view>

namespace nil::sm::id
{
    constexpr std::uint64_t fnv_offset = 14695981039346656037ULL;
    constexpr std::uint64_t fnv_prime = 1099511628211ULL;

    inline void hash_byte(std::uint64_t& hash, std::uint8_t byte)
    {
        hash ^= byte;
        hash *= fnv_prime;
    }

    inline void hash_u64(std::uint64_t& hash, std::uint64_t value)
    {
        for (std::size_t i = 0; i < 8; ++i)
        {
            hash_byte(hash, static_cast<std::uint8_t>((value >> (i * 8U)) & 0xFFU));
        }
    }

    inline void hash_sv(std::uint64_t& hash, std::string_view value)
    {
        for (const auto c : value)
        {
            hash_byte(hash, static_cast<std::uint8_t>(c));
        }
    }

    inline void hash_ancestry(std::uint64_t& hash, const Metadata* metadata)
    {
        if (metadata->parent != nullptr)
        {
            hash_ancestry(hash, metadata->parent);
        }

        hash_u64(hash, metadata->region);
        hash_u64(hash, metadata->state);
    }

    inline std::uint64_t stable_id(const Metadata& metadata)
    {
        auto hash = fnv_offset;
        hash_sv(hash, "nil::sm::uml::stable-id-v1");
        hash_ancestry(hash, &metadata);
        hash_sv(hash, metadata.name);
        return hash;
    }
}
