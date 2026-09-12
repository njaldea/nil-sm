// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "../diagnostics.hpp"
#include "../state.hpp"

#include <iterator>
#include <ostream>
#include <span>

namespace nil::sm::format
{
    using render_function = void (*)(std::ostream&, std::span<const ir::Node>);

    struct root_view
    {
        explicit root_view(const ir::Model* init_model, render_function init_render)
            : model(init_model)
            , render(init_render)
        {
        }

        friend std::ostream& operator<<(std::ostream& os, const root_view& view)
        {
            view.render(os, view.model->roots);
            return os;
        }

    private:
        const ir::Model* model;
        render_function render;
    };

    struct barrier_view
    {
        explicit barrier_view(
            const ir::BarrierDefinition* init_barrier,
            render_function init_render
        )
            : barrier(init_barrier)
            , render(init_render)
        {
        }

        const void* id() const
        {
            return barrier->id;
        }

        std::string_view name() const
        {
            return barrier->name;
        }

        friend std::ostream& operator<<(std::ostream& os, const barrier_view& view)
        {
            view.render(os, view.barrier->roots);
            return os;
        }

    private:
        const ir::BarrierDefinition* barrier;
        render_function render;
    };

    class barrier_range
    {
        class iterator
        {
        public:
            using difference_type = std::ptrdiff_t;
            using value_type = barrier_view;
            using iterator_category = std::forward_iterator_tag;

            value_type operator*() const
            {
                return value_type{&model->barriers[index], render};
            }

            iterator& operator++()
            {
                ++index;
                return *this;
            }

            friend bool operator==(const iterator& left, const iterator& right)
            {
                return left.model == right.model && left.index == right.index;
            }

        private:
            friend class barrier_range;

            iterator(
                const ir::Model* init_model,
                std::size_t init_index,
                render_function init_render
            )
                : model(init_model)
                , index(init_index)
                , render(init_render)
            {
            }

            const ir::Model* model;
            std::size_t index;
            render_function render;
        };

    public:
        explicit barrier_range(const ir::Model* init_model, render_function init_render)
            : model(init_model)
            , render(init_render)
        {
        }

        iterator begin() const
        {
            return iterator{model, 0, render};
        }

        iterator end() const
        {
            return iterator{model, model->barriers.size(), render};
        }

    private:
        const ir::Model* model;
        render_function render;
    };

    template <typename Machine, void (*Render)(std::ostream&, std::span<const ir::Node>)>
    struct diagram;

    template <
        typename API,
        typename T,
        typename... Props,
        void (*Render)(std::ostream&, std::span<const ir::Node>)>
    struct diagram<nil::sm::SM<API, T, Props...>, Render>
    {
        explicit diagram()
            : model(ir::build<API, T>())
            , root{&model, Render}
            , barriers{&model, Render}
        {
            nil::sm::validate(model);
        }

        ~diagram() = default;

        diagram(const diagram&) = delete;
        diagram& operator=(const diagram&) = delete;
        diagram(diagram&&) = delete;
        diagram& operator=(diagram&&) = delete;

        ir::Model model;
        root_view root;
        barrier_range barriers;
    };
}
