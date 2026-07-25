# nil/sml — Advanced: Customising the API Template Parameter

`SM<API, Regions...>` takes an optional first template parameter: a `template <typename State> struct` that controls how states are constructed and how lifecycle hooks are dispatched.

## What the API must provide

For each `State` the SM instantiates, `API<State>` must expose the following type aliases and static methods:

```cpp
template <typename State>
struct MyAPI
{
    // --- Required type aliases ---

    // The state value type (usually just State itself).
    using state_t = State;

    // Type stored in SM and passed as state_context_t* to make().
    // Use void when no context is needed.
    using state_context_t = /* e.g. std::tuple<MyCtx*> or void */;

    // Type stored in SM and passed as api_context_t* to all lifecycle hooks.
    // Use void when no context is needed.
    using api_context_t = /* e.g. std::tuple<MyObserver*> or void */;

    // Regions and events lists (usually coalesced from State itself).
    using regions_t = nil::xalt::coalesce_t<State, nil::sm::detail::regions_tag>;
    using events_t  = nil::xalt::coalesce_t<State, nil::sm::detail::events_tag>;

    // --- Required static methods ---

    // Constructs the state. metadata contains runtime state metadata (for example, region index).
    template <typename Parent>
    static state_t make(
        Parent* parent,
        state_context_t* state_contexts,
        api_context_t* api_contexts,
        const nil::sm::state_metadata& metadata
    );

    // Dispatches an event. Return type must satisfy the on_event return constraints.
    template <typename E>
    static auto on_event(state_t& state, const E& event, api_context_t* api_contexts);

    // Called when the state becomes active.
    static auto on_enter(state_t& state, api_context_t* api_contexts);

    // Called when the state is destroyed.
    static auto on_exit(state_t& state, api_context_t* api_contexts);

    // Called once all direct child regions have terminated.
    static auto on_regions_finalized(state_t& state, api_context_t* api_contexts);
};
```

## Context types and the SM constructor

The SM reads `state_context_t` and `api_context_t` from `API<root<Regions...>>`. The SM constructor takes **pointers** to both context types — the caller owns their lifetime:

```cpp
SM(state_contexts_t* state_contexts, api_contexts_t* api_contexts)
```

where `state_contexts_t = API<root<...>>::state_context_t` and `api_contexts_t = API<root<...>>::api_context_t`.

With the default API both collapse to `void`, so the constructor takes `void*` for both:

```cpp
nil::sm::DefaultSM<Root> sm{nullptr, nullptr};
```

With a custom API whose `state_context_t = std::tuple<MyCtx*>` and `api_context_t = MyObs`:

```cpp
MyCtx ctx;
MyObs obs;
auto state_ctx = std::make_tuple(&ctx);
nil::sm::SM<MyAPI, Root> sm(&state_ctx, &obs);
```

## Three customisation strategies

### 1. Use `default_api` directly with custom context types

The simplest approach. Selects the default `make`, `on_event`, `on_enter`, `on_exit`, and `on_regions_finalized` behaviour, with your chosen context types:

```cpp
template <typename State>
using MyAPI = nil::sm::default_api<
    State,
    MyContext,  // state_context_t (SM will hold MyContext* and pass it to make)
    void        // api_context_t
>;

MyContext ctx;
nil::sm::SM<MyAPI, Root> sm(&ctx, nullptr);
```

`default_api::make` will try to construct the state as `State(parent, static_cast<state_context_t*>(state_contexts))`. States can therefore accept `(Parent*, MyContext*)` in their constructor. If that signature doesn't match, it falls back to `(Parent*)` and then default construction.

### 2. Write a full custom API struct

Implement all static methods manually. Useful when you need to intercept construction, inject observer objects, or observe lifecycle transitions:

```cpp
template <typename State>
struct InstrumentedAPI
{
    using state_t         = State;
    using state_context_t = std::tuple<>;
    using api_context_t   = std::tuple<Observer*>;
    using regions_t = nil::xalt::coalesce_t<State, nil::sm::detail::regions_tag>;
    using events_t  = nil::xalt::coalesce_t<State, nil::sm::detail::events_tag>;

    using base_t = nil::sm::default_api<State, state_context_t, api_context_t>;

    template <typename Parent>
    static state_t make(
        Parent* parent,
        state_context_t* sc,
        api_context_t* ac,
        const nil::sm::state_metadata& metadata
    )
    {
        std::get<0>(*ac)->on_make(nil::xalt::type_id<State>);
        return base_t::make(parent, sc, ac, metadata);
    }

    static auto on_enter(state_t& state, api_context_t* ac)
    {
        std::get<0>(*ac)->on_enter(nil::xalt::type_id<State>);
        return base_t::on_enter(state, ac);
    }

    static auto on_exit(state_t& s, api_context_t* ac) { return base_t::on_exit(s, ac); }

    template <typename E>
    static auto on_event(state_t& s, const E& e, api_context_t* ac) { return base_t::on_event(s, e, ac); }

    static auto on_regions_finalized(state_t& s, api_context_t* ac) { return base_t::on_regions_finalized(s, ac); }
};

Observer obs;
auto api_ctx = std::make_tuple(&obs);
nil::sm::SM<InstrumentedAPI, Root> sm(nullptr, &api_ctx);
```

### 3. Use `coalesce_api` with selective overrides

`nil::sm::coalesce_api<PartialAPI>` takes a **template** (`template <typename T> struct`) and produces a complete API type by filling in any missing methods with `default_api` defaults. Useful when you only need to override one or two hooks.

The resulting API type is `nil::sm::coalesce_api<PartialAPI>::type`. There is also a convenience alias:

```cpp
// CoalescedSM<PartialAPI, Regions...> expands to SM<coalesce_api<PartialAPI>::type, Regions...>
nil::sm::CoalescedSM<PartialAPI, MyRegion> sm{state_contexts, api_contexts};
```

Each method you define in `PartialAPI<T>` replaces the corresponding default. Any method you omit falls back to `default_api<T>`.

```cpp
template <typename T>
struct MyPartialAPI
{
    // Optional: override context types (defaults to void*)
    using api_context_t = MyObserver;

    // Only override on_enter; make, on_event, on_exit, on_regions_finalized fall through
    static auto on_enter(T& state, MyObserver* api_contexts)
    {
        api_contexts->entered();
        return nil::sm::default_api<T>::on_enter(state, nullptr);
    }
};

MyObserver obs;
nil::sm::CoalescedSM<MyPartialAPI, Root> sm{nullptr, &obs};
```

---

## Complete Examples

### Example 1: intercept `make` (observe construction)

```cpp
struct e_tick {};

struct my_state
{
    using events = nil::xalt::tlist<e_tick>;

    auto on_event(const e_tick&) { return nil::sm::Discard{}; }
};

class ConstructionObserver
{
public:
    virtual void on_constructed() = 0;
};

template <typename T>
struct MakeOnlyAPI
{
    using api_context_t = ConstructionObserver;

    template <typename Parent>
    static T make(
        Parent* parent,
        void* state_contexts,
        ConstructionObserver* api_contexts,
        const nil::sm::state_metadata& metadata
    )
    {
        if constexpr (!std::is_same_v<T, nil::sm::fin>)
        {
            api_contexts->on_constructed();
        }
        return nil::sm::default_api<T>::make(parent, state_contexts, nullptr, metadata);
    }
    // on_event, on_enter, on_exit, on_regions_finalized — not defined; fall through to defaults
};

// Usage:
ConstructionObserver obs;
nil::sm::CoalescedSM<MakeOnlyAPI, my_state> sm{nullptr, &obs};
sm.post(e_tick{});
```

### Example 2: intercept `on_enter` (observe state activation)

```cpp
class EnterObserver
{
public:
    virtual void on_entered() = 0;
};

template <typename T>
struct EnterOnlyAPI
{
    using api_context_t = EnterObserver;

    static auto on_enter(T& state, EnterObserver* api_contexts)
    {
        if constexpr (!std::is_same_v<T, nil::sm::fin>)
        {
            api_contexts->on_entered();
        }
        return nil::sm::default_api<T>::on_enter(state, nullptr);
    }
    // make, on_event, on_exit, on_regions_finalized — not defined; fall through to defaults
};

// Usage:
EnterObserver obs;
nil::sm::CoalescedSM<EnterOnlyAPI, my_state> sm{nullptr, &obs};
```

### Example 3: intercept `on_event` (observe every dispatched event)

```cpp
class EventObserver
{
public:
    virtual void on_event_dispatched() = 0;
};

template <typename T>
struct EventOnlyAPI
{
    using api_context_t = EventObserver;

    template <typename E>
    static auto on_event(T& state, const E& event, EventObserver* api_contexts)
    {
        if constexpr (!std::is_same_v<T, nil::sm::fin>)
        {
            api_contexts->on_event_dispatched();
        }
        return nil::sm::default_api<T>::template on_event<E>(state, event, nullptr);
    }
    // make, on_enter, on_exit, on_regions_finalized — not defined; fall through to defaults
};

// Usage:
EventObserver obs;
nil::sm::CoalescedSM<EventOnlyAPI, my_state> sm{nullptr, &obs};
sm.post(e_tick{}); // calls obs.on_event_dispatched() once
```

### Example 4: spread a tuple state context into constructor arguments

When a state expects multiple context pointers individually rather than a single tuple pointer, override `make` to unpack the tuple via `std::apply`:

```cpp
struct CtxA { int a = 0; };
struct CtxB { int b = 0; };

struct spread_state
{
    using events = nil::xalt::tlist<e_tick>;

    int sum = 0;

    explicit spread_state(auto* /*parent*/, CtxA* a, CtxB* b)
        : sum(a->a + b->b)
    {}

    auto on_event(const e_tick&) const { return nil::sm::Discard{}; }
};

template <typename T>
struct SpreadMakeAPI
{
    using state_context_t = std::tuple<CtxA*, CtxB*>;

    template <typename Parent>
    static T make(
        Parent* parent,
        state_context_t* state_contexts,
        void* /*api_contexts*/,
        const nil::sm::state_metadata& /* metadata */
    )
    {
        if constexpr (!std::is_same_v<T, nil::sm::fin>)
        {
            return std::apply(
                [parent](auto*... args) -> T { return T(parent, args...); },
                *state_contexts
            );
        }
        else
        {
            return T{};
        }
    }
    // on_event, on_enter, on_exit, on_regions_finalized — not defined; fall through to defaults
};

// Usage:
CtxA a{.a = 10};
CtxB b{.b = 32};
auto ctx = std::tuple<CtxA*, CtxB*>(&a, &b);
nil::sm::CoalescedSM<SpreadMakeAPI, spread_state> sm{&ctx, nullptr};
// spread_state constructed with sum == 42
```
