#pragma once

#include "../ir.hpp"
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

    struct provider_view
    {
        explicit provider_view(const ir::Provider* init_provider, render_function init_render)
            : provider(init_provider)
            , render(init_render)
        {
        }

        const void* id() const
        {
            return provider->id;
        }

        std::string_view name() const
        {
            return provider->name;
        }

        friend std::ostream& operator<<(std::ostream& os, const provider_view& view)
        {
            view.render(os, view.provider->roots);
            return os;
        }

    private:
        const ir::Provider* provider;
        render_function render;
    };

    class provider_range
    {
        class iterator
        {
        public:
            using difference_type = std::ptrdiff_t;
            using value_type = provider_view;
            using iterator_category = std::forward_iterator_tag;

            value_type operator*() const
            {
                return value_type{&model->providers[index], render};
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
            friend class provider_range;

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
        explicit provider_range(const ir::Model* init_model, render_function init_render)
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
            return iterator{model, model->providers.size(), render};
        }

    private:
        const ir::Model* model;
        render_function render;
    };

    template <typename Machine, void (*Render)(std::ostream&, std::span<const ir::Node>)>
    struct diagram;

    template <
        template <typename>
        typename API,
        typename T,
        void (*Render)(std::ostream&, std::span<const ir::Node>)>
    struct diagram<nil::sm::SM<API, T>, Render>
    {
        explicit diagram()
            : model(ir::build<API, T>())
            , root{&model, Render}
            , providers{&model, Render}
        {
        }

        ~diagram() = default;

        diagram(const diagram&) = delete;
        diagram& operator=(const diagram&) = delete;
        diagram(diagram&&) = delete;
        diagram& operator=(diagram&&) = delete;

        const ir::Model model;
        root_view root;
        provider_range providers;
    };
}
