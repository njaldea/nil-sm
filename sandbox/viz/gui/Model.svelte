<!--
@component
One state machine's diagram: the states of a machine (the root tree, or one barrier body) laid out
as boxes and transitions in an SVG.

It loads the states a level at a time, lays them out, and draws them. Clicking a collapsed state
opens it, and the diagram narrows to that state; clicking it again collapses it and shows its
parent. That change is animated: the state's box moves between its collapsed and expanded size
while the page scrolls along with it, and it can be turned around mid-way.

Props:
  root_props  the props the root provides, for the root's unmet args
  focus       id of the barrier to show, or null for the root tree
  theme       "light" or "dark"
  show_props  whether each state lists the props it provides
  zoom_speed  how fast the move is, as a multiple of the default

How the pieces fit:
  tree.js      fetches the states        layout.js    places them (with routing.js)
  scenes.js    decides what to draw      motion.js    easing and the following scroll
  StateNode, Edge, BoxFrame, ArrowMarkers draw it; Styles.svelte styles it
-->

<script>
    import { setContext, tick } from "svelte";
    import ArrowMarkers from "./ArrowMarkers.svelte";
    import BoxFrame from "./BoxFrame.svelte";
    import Edge from "./Edge.svelte";
    import StateNode from "./StateNode.svelte";
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
    import { ease, make_follow } from "./motion.js";
    import { build_scenes } from "./scenes.js";
    import { build_tree, fetch_json } from "./tree.js";

    let { root_props = [], focus = null, theme = "light", show_props = false, zoom_speed = 1 } = $props();

    const elk = new window.ELK();

    // ---- what is shown ----

    // the laid out states that are shown, or null before the first layout
    let layout = $state(null);
    let error = $state(null);

    // ids of the states and barrier hosts that are open; every other one is collapsed
    let open = $state([]);

    // the open state the diagram is narrowed to, or null for the whole diagram
    let focused = $state(null);
    // the `focused` of the layout that is shown (it trails `focused` while the next one loads)
    let shown_focus = $state(null);

    // where each state of the shown layout sits: its parent and its region
    let placement = new Map();

    // ---- the move between a collapsed and an expanded state ----

    // the layout being left, drawn with the shown one while `progress` runs from 0 to 1
    let previous = $state.raw(null);
    let progress = $state(1);
    let frame = 0;

    // the diagram's svg, to scroll along with the moving state
    let svg = $state(null);

    // scrolls the page for a given progress
    let follow = () => {};
    // where the move being played is heading (1: the focused view, 0: the one it left)
    let target = 1;
    let playing = false;
    // the layout that came in during a move; it is taken when the move is over
    let arrived = null;

    // Runs `progress` from where it is to `to`, taking the share of the move that is left.
    const play = (to) => {
        cancelAnimationFrame(frame);
        target = to;
        playing = true;
        const origin = progress;
        const duration = (ZOOM.MS / zoom_speed) * Math.abs(to - origin);
        const start = performance.now();
        const step = (now) => {
            const t = duration === 0 ? 1 : Math.min(1, (now - start) / duration);
            progress = t === 1 ? to : origin + (to - origin) * t;
            follow(progress);
            if (t < 1) {
                frame = requestAnimationFrame(step);
            } else {
                playing = false;
                settle();
            }
        };
        frame = requestAnimationFrame(step);
    };

    // Ends the move once it has stopped: the layout that came in is taken, or the left one is dropped.
    const settle = () => {
        if (playing) {
            return;
        }

        // the layout shown where the move ended
        const end_focus = progress === 1 ? shown_focus : previous?.focus;
        if (arrived && arrived.focus === end_focus) {
            layout = arrived.result;
            placement = arrived.placed;
            shown_focus = arrived.focus;
            arrived = null;
            previous = null;
            progress = 1;
        } else if (progress === 1) {
            arrived = null;
            previous = null;
        }
    };

    // A new layout came in while a move is playing. If it is the layout the move left, or the one it
    // heads for, the move goes there from where it is. Returns false for any other layout.
    const receive = (result, placed, focus_id) => {
        const to = focus_id === previous.focus ? 0 : focus_id === shown_focus ? 1 : null;
        if (to === null) {
            return false;
        }

        arrived = { result, placed, focus: focus_id };
        if (to === target) {
            settle();
        } else {
            play(to);
        }
        return true;
    };

    // The box a state is drawn in within a flattened layout.
    const state_rect = (flat_nodes, id) => {
        const node = flat_nodes.find((n) => n.kind === "state" && n.data?.id === id);
        return node && { x: node.x, y: node.y, w: node.width, h: node.height, data: node.data };
    };

    // Starts the move from the shown layout to `next`, if a state is expanding or collapsing.
    // Opening grows the state's box from its collapsed size to the focused view; collapsing
    // shrinks it back. `before` and `after` are the flattened layouts.
    const begin_zoom = (before, after, focus_id, next) => {
        cancelAnimationFrame(frame);
        playing = false;
        arrived = null;
        previous = null;
        progress = 1;

        if (
            !layout ||
            focus_id === shown_focus ||
            window.matchMedia?.("(prefers-reduced-motion: reduce)").matches
        ) {
            return;
        }

        let move = null;
        if (focus_id != null) {
            const small = state_rect(before.nodes, focus_id);
            const big = state_rect(after.nodes, focus_id);
            if (small && big) {
                move = { opening: true, id: focus_id, small, big };
            }
        }
        if (!move && shown_focus != null) {
            const big = state_rect(before.nodes, shown_focus);
            const small = state_rect(after.nodes, shown_focus);
            if (small && big) {
                move = { opening: false, id: shown_focus, small, big };
            }
        }
        if (!move) {
            return;
        }

        previous = {
            ...move,
            focus: shown_focus,
            width: layout.width ?? 0,
            height: layout.height ?? 0,
            containers: before.nodes.filter(is_container),
            edges: before.edges,
            leaves: before.nodes.filter((node) => !is_container(node)),
        };
        progress = 0;

        follow = make_follow({
            svg,
            move,
            sizes: {
                before: { width: previous.width, height: previous.height },
                after: { width: next.width ?? 0, height: next.height ?? 0 },
            },
            heading: () => target,
            tick,
        });

        play(1);
    };

    $effect(() => () => cancelAnimationFrame(frame));

    // ---- opening and collapsing states ----

    const toggleable = (data) =>
        data.id !== "root" &&
        (data.expandable ||
            data.collapsed ||
            (data.regions?.length ?? 0) > 0);

    const is_below = (id, ancestor) => {
        for (
            let parent = placement.get(id)?.parent;
            parent != null;
            parent = placement.get(parent)?.parent
        ) {
            if (parent === ancestor) {
                return true;
            }
        }
        return false;
    };

    // Opening a state unloads the opened state of its region, and with it everything below.
    const toggle_node = (data) => {
        if (!toggleable(data)) {
            return;
        }

        // The box of a state that is moving turns around at once, before its layout is ready.
        if (previous && data.id === previous.id) {
            arrived = null;
            play(target === 1 ? 0 : 1);
        }

        const opened = new Set(open);
        const unload = (id) => {
            for (const other of [...opened]) {
                if (other === id || is_below(other, id)) {
                    opened.delete(other);
                }
            }
        };

        if (opened.has(data.id)) {
            if (focused != null && (focused === data.id || is_below(focused, data.id))) {
                const parent = placement.get(data.id)?.parent;
                focused = parent == null || parent === "root" ? null : parent;
            }
            unload(data.id);
        } else {
            const region = placement.get(data.id)?.region;
            for (const other of [...opened]) {
                if (region != null && placement.get(other)?.region === region) {
                    unload(other);
                }
            }
            opened.add(data.id);
            focused = data.id;
        }
        open = [...opened];
    };

    // ---- loading ----

    // what a barrier shown on its own leaves for its host to provide
    let requirements = $state([]);

    $effect(() => {
        if (focus == null) {
            return;
        }

        let cancelled = false;

        fetch_json(`/model_val/${focus}`)
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
        // the layout depends on the rows, which depend on this
        void show_props;

        const open_ids = new Set(open);
        const focus_id = focused;
        let cancelled = false;

        // The previous layout stays on screen until the levels this one needs have arrived.
        build_tree({ focus, root_props, open_ids, focused: focus_id })
            .then(async (tree) => ({
                result: await layout_graph(tree.roots, { elk, show_props }),
                placed: tree.placement,
            }))
            .then(({ result, placed }) => {
                if (cancelled) {
                    return;
                }

                if (previous && receive(result, placed, focus_id)) {
                    return;
                }

                begin_zoom(flat, flatten_layout(result), focus_id, result);
                layout = result;
                placement = placed;
                shown_focus = focus_id;
                error = null;
            })
            .catch((exception) => {
                if (cancelled) {
                    return;
                }

                layout = null;
                error = String(exception);
            });

        return () => {
            cancelled = true;
        };
    });

    // ---- what the template draws ----

    const flat = $derived(layout ? flatten_layout(layout) : { nodes: [], edges: [] });
    const edges = $derived(flat.edges);

    // id of the edge under the pointer
    let hovered = $state(null);

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

    const hovered_edge = $derived(
        edges.find((edge) => edge.id === hovered)
    );

    // Containers are drawn under the edges so their fill does not hide the transitions inside.
    const is_container = (node) =>
        node.kind === "region" ||
        node.composite === true;

    const containers = $derived(flat.nodes.filter(is_container));
    const leaves = $derived(flat.nodes.filter((node) => !is_container(node)));

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
        },
        toggleable,
        toggle: toggle_node,
    });

    const eased = $derived(ease(progress));

    const scenes = $derived(
        build_scenes({ focus: shown_focus, containers, edges, leaves }, previous, progress)
    );
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
    {:else if layout}
        <!--
            During a move the svg keeps the larger of the two sizes, so the page under the scroll does
            not shrink; the canvas behind the states is drawn at the size in between.
        -->
        {@const width = Math.max(layout.width ?? 0, previous?.width ?? 0)}
        {@const height = Math.max(layout.height ?? 0, previous?.height ?? 0)}
        {@const canvas_w = previous ? previous.width + ((layout.width ?? 0) - previous.width) * eased : width}
        {@const canvas_h = previous ? previous.height + ((layout.height ?? 0) - previous.height) * eased : height}
        <svg bind:this={svg} {width} {height} viewBox={`0 0 ${width} ${height}`}>
            <defs>
                <ArrowMarkers />
            </defs>

            {#snippet scene_shapes(scene)}
                {#each scene.containers as node (node.id)}
                    <StateNode {node} outer_id={scene.focus} />
                {/each}

                <!-- edges are drawn over the containers -->
                {#each scene.edges as edge (edge.id)}
                    <Edge
                        {edge}
                        onenter={() => (hovered = edge.id)}
                        onleave={() => (hovered = null)}
                    />
                {/each}

                <!-- the hovered edge is drawn again on top, so reordering never disturbs the hover -->
                {#if scene.live && hovered_edge}
                    <Edge edge={hovered_edge} hot />
                {/if}
            {/snippet}

            <clipPath id="canvas-clip">
                <rect width={canvas_w} height={canvas_h} rx="6" />
            </clipPath>

            <rect
                class="canvas"
                x="0.5"
                y="0.5"
                width={Math.max(0, canvas_w - 1)}
                height={Math.max(0, canvas_h - 1)}
                rx="5.5"
            />

            <g clip-path="url(#canvas-clip)">
                {#each scenes as scene (scene.key)}
                    {#if scene.kind === "box" || scene.kind === "chrome"}
                        <BoxFrame {scene} part={scene.kind} />
                    {:else if scene.kind === "inside"}
                        <clipPath id="morph-clip">
                            <rect
                                x={scene.box.x}
                                y={scene.box.y}
                                width={scene.box.w}
                                height={scene.box.h}
                                rx="6"
                            />
                        </clipPath>

                        <g clip-path="url(#morph-clip)">
                            <g
                                class="stale"
                                transform={`translate(${scene.shift.x} ${scene.shift.y})`}
                                opacity={scene.opacity}
                            >
                                {@render scene_shapes(scene)}

                                {#each scene.leaves as node (node.id)}
                                    <StateNode {node} outer_id={scene.focus} />
                                {/each}
                            </g>
                        </g>
                    {:else}
                        <g class:stale={!scene.live} opacity={scene.opacity}>
                            {@render scene_shapes(scene)}

                            {#each scene.leaves as node (node.id)}
                                <StateNode {node} outer_id={scene.focus} />
                            {/each}
                        </g>
                    {/if}
                {/each}
            </g>
        </svg>
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

    svg {
        font-family:
            system-ui,
            -apple-system,
            BlinkMacSystemFont,
            "Segoe UI",
            Roboto,
            sans-serif;

        font-size: 11px;
    }

    rect.canvas {
        fill: var(--canvas);
        stroke: var(--canvas-edge);
        stroke-width: 1;
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
