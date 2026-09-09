# Extensibility

Most applications need only a state type, `DefaultSM`, and `post()`. Use this
page when states need application objects or when the machine must be observed
or type-erased.

## Contexts and state arguments

State dependencies are non-owning. List their types in `args`; pass root
arguments as pointers when constructing the machine:

```cpp
struct AppContext
{
    int user_id;
};

struct logged_in
{
    using args = nil::xalt::tlist<AppContext>;

    explicit logged_in(AppContext* app)
        : user_id(app->user_id) {}

    int user_id;
};

AppContext app{42};
nil::sm::DefaultSM<logged_in, AppContext> machine{&app};
```

The machine stores addresses. Keep context and root-argument objects alive until
it is destroyed.

Use `props` when a parent deliberately exposes a member to descendants:

```cpp
struct parent
{
    AppContext app;
    using props = nil::xalt::tlist<
        nil::sm::prop<AppContext, &parent::app>>;
    using regions = nil::xalt::tlist<child>;
};
```

`direct_parent<T>` is an explicit escape hatch for the immediate parent. It is
not available across a barrier boundary.

## Type erasure

Use `ISM` when callers should not know the concrete root state or API:

```cpp
std::unique_ptr<nil::sm::ISM> make_machine(AppContext* app)
{
    return std::make_unique<nil::sm::DefaultSM<logged_in, AppContext>>(app);
}

AppContext app{42};
auto machine = make_machine(&app);
machine->post(start{});
```

`ISM` is non-copyable and has a virtual destructor. The machine owns its state
objects; context and root arguments remain borrowed.

## Custom hooks

A custom API is a template `API<State>`. Use `Coalesce` when only selected hooks
need to change:

```cpp
struct Observer
{
    void entered(const void* state_id);
};

template <typename State>
struct LoggingAPI
{
    using api_context_t = Observer;

    static auto on_enter(State& state, Observer* observer)
    {
        if constexpr (!std::is_same_v<State, nil::sm::Fin>)
            observer->entered(nil::xalt::type_id<State>);
        return nil::sm::api::Default<Observer>::type<State>::on_enter(
            state,
            observer
        );
    }
};

AppContext app{42};
Observer observer;
nil::sm::CoalescedSM<LoggingAPI, logged_in, AppContext> machine{&observer, &app};
```

`Coalesce` supplies missing aliases and hooks from `api::Default`. Delegate to
`Default` when the normal state behavior should remain active. The complete
hook contract is in [Advanced API](05_ADVANCED.md).

## Threading and services

`post()` is synchronous and the library is not thread-safe. A common integration
is an application queue drained by one owner thread. Custom APIs can also add
logging, profiling, timers, allocation, or other services through `api_context_t`
and `make()`.
