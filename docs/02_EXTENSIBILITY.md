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

Use `props` when a parent deliberately exposes a non-owning pointer to
descendants. A child lists the exposed type in `args` and receives `T*` in its
constructor. A value data member is exposed by address:

```cpp
struct child
{
    using args = nil::xalt::tlist<AppContext>;

    explicit child(AppContext* app)
        : user_id(app->user_id) {}

    int user_id;
};

struct parent
{
    AppContext app;
    using props = nil::xalt::tlist<
        nil::sm::prop<AppContext, &parent::app>>;
    using regions = nil::xalt::tlist<child>;
};
```

`prop<T, Accessor>` requires a mutable, non-const `T` and exposes `T*`. Its
accessor may be one of the following forms:

```cpp
T parent::*member;                  // value member: T C::*, use prop<T, ...>
T* parent::*member;                 // pointer member: T* C::*, use prop<T*, ...>
T* parent::get_member();            // non-const member function: T* (C::*)()
T* get_member(parent&);             // free function: T* (*)(C&)
```

Data members are always exposed by address. Therefore, a `T C::*` property
resolves to `T*`, while a `T* C::*` property resolves to `T**`. A child that
requests the latter uses `args = tlist<T*>` and receives `T**`.

The member function and free function receive mutable state. A `const` member
function, a free function taking `const C&`, and a `prop<const T, ...>` are not
valid property accessors.

`direct_parent<T>` is an explicit escape hatch for the immediate parent. Only that parent's
own props are searched, so it must expose `T`; `as_parent<Base>` exposes the state itself
as its base type:

```cpp
struct parent: base
{
    using props = nil::xalt::tlist<nil::sm::as_parent<base>>;
};

struct child
{
    using args = nil::xalt::tlist<nil::sm::direct_parent<base>>;
};
```

It is not available across a barrier boundary.

## Argument validation in diagrams

`ir::build()` returns a plain model: `roots` (the states of the top-level region) and
`barriers` (each barrier definition with its own `roots`). Every node lists the
`required_args` it needs and the `provided_props` it offers; the model holds no
validation results, because a shared barrier can be satisfied at one host and not at
another.

`nil::sm::validate` answers pass/fail: does every state get its args from an ancestor
prop or a root arg?

```cpp
auto model = nil::sm::ir::build<parent>();
assert(nil::sm::validate(model));
```

Arguments handed to the `SM` constructor are not props of any state. List their
types so root requirements are satisfied:

```cpp
static_assert(nil::sm::validate<parent, api, app_context>());
assert(nil::sm::validate(model, nil::sm::ir::make_root_props<app_context>()));
```

A barrier looks props up through its host but never reaches `direct_parent`, so its
requirements are checked against each host's ancestors. To see which state is missing
what, use the `viz` sandbox: it evaluates the model per occurrence and lists the
unmet args on each state, barrier host, and root.

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

A custom API provides a nested `state<State>` template. Use `Coalesce` when only
selected hooks need to change:

```cpp
struct Observer
{
    void entered(const void* state_id);
};

struct LoggingAPI
{
    using context_t = Observer;

    template <typename State>
    struct state
    {
        static auto on_enter(State& state, Observer* observer)
        {
            if constexpr (!std::is_same_v<State, nil::sm::Fin>)
                observer->entered(nil::xalt::type_id<State>);
            return nil::sm::api::Default<Observer>::state<State>::on_enter(
                state,
                observer
            );
        }
    };
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
logging, profiling, timers, allocation, or other services through `context_t`
and `make()`.
