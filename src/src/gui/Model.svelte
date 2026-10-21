<!--
@component
One state machine's diagram: the states of a machine (the root tree, or one barrier body), shown
as panes side by side, each pane the svg of one opened state.

It loads the states a level at a time and lays out one pane per opened state with its children
collapsed. Only the last two opened states are shown: a state and its parent. Opening a state
slides the panes to the left, so the parent that is no longer needed leaves on the left; collapsing
it slides them back.

Props:
  root_props  the props the root provides, for the root's unmet args
  focus       id of the barrier to show, or null for the root tree
  theme       "light" or "dark"
  show_props  whether each state lists the props it provides
  zoom_speed  how fast the slide is, as a multiple of the default

How the pieces fit:
  tree.js      fetches the states        layout.js    places them (with routing.js)
  motion.js    easing of the slide       Pane.svelte  draws one pane, with StateNode, Edge and
                                         ArrowMarkers; Styles.svelte styles them
-->

<script>
    import "elkjs/lib/elk.bundled.js";

    import { setContext } from "svelte";
    import Pane from "./Pane.svelte";
    import Styles from "./Styles.svelte";
    import { COMPOSITE, NODE, ROWS, ZOOM } from "./constants.js";
    import { flatten_layout, layout_graph } from "./layout.js";
    import {
        action_texts,
        actions_height,
        block_height,
        missing_texts,
        prop_texts,
    } from "./measure.js";
    import { ease } from "./motion.js";
    import { build_tree, fetch_json } from "./tree.js";

    let { root_props = [], focus = null, theme = "light", show_props = false, zoom_speed = 1 } = $props();

    const elk = new window.ELK();

    // space between two panes
    const GAP = 16;

    // ---- what is shown ----

    let error = $state(null);

    // the opened state whose pane is the last one, or null for the root alone
    let focused = $state(null);

    // the panes in the page, left to right: { key, id, layout, flat }
    let shown = $state.raw([]);

    // where each state sits: its parent and its region
    const placement = new Map();

    // the states from the top down to `id`, not counting the root
    const chain_of = (id) => {
        const ids = [];
        for (let at = id; at != null && at !== "root"; at = placement.get(at)?.parent) {
            ids.unshift(at);
        }
        return ids;
    };

    // the states that are open: the path down to the focused one
    const opened = $derived(new Set(chain_of(focused)));

    // ---- the panes ----

    // pane layouts that were made; they do not depend on what else is open
    const cache = new Map();
    let cached_props = show_props;

    // The pane of a state (null: the root) with its children collapsed.
    const load_pane = (id) => {
        const key = id ?? "root";
        let pane = cache.get(key);
        if (!pane) {
            pane = build_tree({ focus, root_props, open_ids: new Set(chain_of(id)), pane: id })
                .then(async (tree) => {
                    for (const [state, where] of tree.placement) {
                        placement.set(state, where);
                    }
                    const layout = await layout_graph(tree.roots, { elk, show_props });
                    return { key, id, layout, flat: flatten_layout(layout) };
                })
                .catch((failure) => {
                    cache.delete(key);
                    throw failure;
                });
            cache.set(key, pane);
        }
        return pane;
    };

    // ---- sliding the panes ----

    // the offset of the panes and the width of what is shown of them, while they slide
    let offset = $state(0);
    let view_width = $state(null);
    let moving = $state(false);

    let frame = 0;
    // ends the slide that is playing
    let finish = null;

    const width_of = (panes) =>
        panes.reduce((sum, pane, i) => sum + (pane.layout.width ?? 0) + (i > 0 ? GAP : 0), 0);

    // how many states are open below the root in what is shown
    let shown_depth = 0;

    // Shows `next`. Panes that leave or come back slide, in the direction of `depth`; a change that
    // keeps one of the panes in another place is at once.
    const go = (next, depth) => {
        finish?.();

        const before = shown;
        const before_depth = shown_depth;
        shown_depth = depth;

        const reduced = window.matchMedia?.("(prefers-reduced-motion: reduce)").matches;
        const same = (a, b) => a.key === b.key;

        // forward: the panes move left; `away` are the ones that leave or come in
        let forward = true;
        let joined = null;
        let away = [];
        if (before.length === 2 && next.length === 2 && same(before[1], next[0])) {
            joined = [before[0], ...next];
            away = [before[0]];
        } else if (before.length === 2 && next.length === 2 && same(before[0], next[1])) {
            forward = false;
            joined = [...next, before[1]];
            away = [next[0]];
        } else if (before.length > 0 && !before.some((a) => next.some((b) => same(a, b)))) {
            forward = depth > before_depth;
            joined = forward ? [...before, ...next] : [...next, ...before];
            away = forward ? before : next;
        }

        if (reduced || !joined) {
            shown = next;
            return;
        }

        const shift = width_of(away) + GAP;
        const from = forward ? 0 : -shift;
        const to = forward ? -shift : 0;
        const from_width = width_of(before);
        const to_width = width_of(next);

        shown = joined;
        offset = from;
        view_width = from_width;
        moving = true;

        finish = () => {
            cancelAnimationFrame(frame);
            finish = null;
            shown = next;
            offset = 0;
            view_width = null;
            moving = false;
        };

        const duration = ZOOM.MS / zoom_speed;
        const start = performance.now();
        const step = (now) => {
            const t = Math.min(1, (now - start) / duration);
            const e = ease(t);
            offset = from + (to - from) * e;
            view_width = from_width + (to_width - from_width) * e;
            if (t < 1) {
                frame = requestAnimationFrame(step);
            } else {
                finish?.();
            }
        };
        frame = requestAnimationFrame(step);
    };

    $effect(() => () => {
        cancelAnimationFrame(frame);
    });

    // ---- opening and collapsing states ----

    const toggleable = (data) =>
        data.id !== "root" &&
        (data.expandable ||
            data.collapsed ||
            (data.regions?.length ?? 0) > 0);

    // Opening a state shows it beside its parent; collapsing an open one goes back to its parent.
    const toggle_node = (data) => {
        if (!toggleable(data)) {
            return;
        }

        if (opened.has(data.id)) {
            const parent = placement.get(data.id)?.parent;
            focused = parent == null || parent === "root" ? null : parent;
        } else {
            focused = data.id;
        }
    };

    // ---- loading ----

    // what a barrier shown on its own leaves for its host to provide
    let requirements = $state([]);

    $effect(() => {
        if (focus == null) {
            return;
        }

        let cancelled = false;

        fetch_json(`model_val/${focus}.json`)
            .then((body) => {
                if (!cancelled) {
                    requirements = body.requires;
                }
            })
            .catch((exception) => {
                if (!cancelled) {
                    error = String(exception);
                }
            });

        return () => {
            cancelled = true;
        };
    });

    $effect(() => {
        // the layouts depend on the rows, which depend on this
        if (cached_props !== show_props) {
            cached_props = show_props;
            cache.clear();
        }

        const chain = chain_of(focused);
        const wanted = [null, ...chain].slice(-2);
        let cancelled = false;

        // What is shown stays until the panes the next view needs have arrived.
        Promise.all(wanted.map(load_pane))
            .then((panes) => {
                if (!cancelled) {
                    error = null;
                    go(panes, chain.length);
                }
            })
            .catch((exception) => {
                if (!cancelled) {
                    shown = [];
                    error = String(exception);
                }
            });

        return () => {
            cancelled = true;
        };
    });

    // ---- hover ----

    // barrier_id of the barrier state under the pointer; every host of it is highlighted
    let hovered_barrier = $state(null);

    // id of the state under the pointer
    let hovered_state = $state(null);

    const enter = (data) => {
        hovered_state = data.id;
        if (data.is_barrier) {
            hovered_barrier = data.barrier_id;
        }
    };

    const leave = () => {
        hovered_state = null;
        hovered_barrier = null;
    };

    const is_peer = (node) =>
        hovered_barrier != null &&
        node.data.is_barrier &&
        node.data.barrier_id === hovered_barrier;

    // what the components that draw the states need from here
    setContext("viz", {
        NODE,
        ROWS,
        COMPOSITE,
        measure: {
            action_texts,
            missing_texts,
            prop_texts: (node) => prop_texts(node, show_props),
            block_height,
            actions_height: (node) => actions_height(node, show_props),
        },
        hover: {
            enter,
            leave,
            is_peer,
            is_focus: (data) => hovered_state === data.id,
            state: () => hovered_state,
        },
        toggleable,
        toggle: toggle_node,
        is_open: (data) => opened.has(data.id),
    });
</script>

<div class="model-container" class:dark={theme === "dark"}>
    {#if focus != null && requirements.length > 0}
        <div class="requirements">
            requires:
            {[...new Set(requirements.map((entry) => entry.dependency.type_name))].join(", ")}
        </div>
    {/if}
    {#if error}
        <pre class="error">{error}</pre>
    {:else if shown.length > 0}
        <div
            class="viewport"
            class:moving
            style:width={view_width == null ? null : `${view_width}px`}
        >
            <div class="strip" style:gap="{GAP}px" style:transform="translateX({offset}px)">
                {#each shown as pane (pane.key)}
                    <Pane {pane} />
                {/each}
            </div>
        </div>
    {/if}
</div>

<Styles />

<style>
    .model-container {
        --canvas: #ffffff;
        --canvas-edge: #e2e8f0;
        --state: #ffffff;
        --composite: #f8fafc;
        --region: #ffffff;
        --stroke: #475569;
        --stroke-strong: #334155;
        --stroke-soft: #94a3b8;
        --text: #1e293b;
        --text-strong: #334155;
        --text-muted: #64748b;
        --edge: #475569;
        --edge-label: #0284c7;
        --edge-hot: #2563eb;
        --opened-stroke: #ea580c;
        --terminal: #0f172a;
        --barrier-fill: #eef2ff;
        --barrier-stroke: #6366f1;
        --peer-fill: #fce7f3;
        --peer-stroke: #db2777;
        --collapsed-stroke: #0891b2;
        --missing-text: #dc2626;
        --error-border: #fecaca;
        --error-bg: #fef2f2;
        --error-text: #ef4444;

        color-scheme: light;
        color: var(--text);

        display: flex;
        flex-direction: column;
        gap: 8px;
        align-items: flex-start;
        max-width: 100%;
    }

    .model-container.dark {
        --canvas: #0f0f0f;
        --canvas-edge: #262626;
        --state: #262626;
        --composite: #1a1a1a;
        --region: #121212;
        --stroke: #a3a3a3;
        --stroke-strong: #d4d4d4;
        --stroke-soft: #525252;
        --text: #e5e5e5;
        --text-strong: #d4d4d4;
        --text-muted: #a3a3a3;
        --edge: #a3a3a3;
        --edge-label: #5eead4;
        --edge-hot: #facc15;
        --opened-stroke: #fb923c;
        --terminal: #e5e5e5;
        --barrier-fill: #2a2433;
        --barrier-stroke: #a78bfa;
        --peer-fill: #3a1f2e;
        --peer-stroke: #f472b6;
        --collapsed-stroke: #5eead4;
        --missing-text: #f87171;
        --error-border: #7f1d1d;
        --error-bg: #3b0a0a;
        --error-text: #fca5a5;

        color-scheme: dark;
    }

    .viewport {
        max-width: 100%;
        overflow-x: auto;
    }

    /* the panes leave and enter past the edges while sliding */
    .viewport.moving {
        max-width: none;
        overflow: hidden;
    }

    .strip {
        display: flex;
        align-items: flex-start;
        width: max-content;
    }

    .strip > :global(svg) {
        animation: enter 200ms ease-out;
    }

    @keyframes enter {
        from {
            opacity: 0;
        }
    }

    .requirements {
        font-size: 11px;
        color: var(--text-muted);
    }

    .error {
        padding: 10px;

        border: 1px solid var(--error-border);
        border-radius: 4px;

        background: var(--error-bg);
        color: var(--error-text);

        font-size: 12px;
    }
</style>
