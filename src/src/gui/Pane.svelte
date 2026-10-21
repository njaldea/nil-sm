<!--
@component
One pane of the viewer: the svg of a single state with its states and transitions drawn in it
(the whole tree for the root). Model.svelte lays panes side by side and slides them.

The hover of transitions and start/end markers is kept here, since both live in this pane's svg.
-->

<script>
    import { getContext, setContext } from "svelte";
    import ArrowMarkers from "./ArrowMarkers.svelte";
    import Edge from "./Edge.svelte";
    import StateNode from "./StateNode.svelte";

    // pane: { id, layout, flat } of a state; id is null for the root
    let { pane } = $props();

    const uid = $props.id();
    const viz = getContext("viz");

    const is_container = (node) => node.kind === "region" || node.composite === true;

    // Containers are drawn under the edges so their fill does not hide the transitions inside.
    const containers = $derived(pane.flat.nodes.filter(is_container));
    const leaves = $derived(pane.flat.nodes.filter((node) => !is_container(node)));
    const edges = $derived(pane.flat.edges);

    // id of the edge under the pointer
    let hovered = $state(null);

    // id of the start or end marker under the pointer
    let hovered_marker = $state(null);

    // The transitions to highlight: the one under the pointer, the one of a hovered start marker,
    // and every one into or out of the hovered state.
    const hot_edges = $derived.by(() => {
        const state = viz.hover.state();
        return edges.filter(
            (edge) =>
                edge.id === hovered ||
                (hovered_marker != null && edge.sources[0] === hovered_marker) ||
                (state != null && (edge.sources[0] === state || edge.targets[0] === state))
        );
    });

    // the states a hovered transition (or a hovered start marker's) runs between
    const ends = $derived(
        new Set(
            edges
                .filter(
                    (edge) =>
                        edge.id === hovered ||
                        (hovered_marker != null && edge.sources[0] === hovered_marker)
                )
                .flatMap((edge) => [edge.sources[0], edge.targets[0]])
        )
    );

    // a marker is lit when it is hovered or an end of a highlighted transition
    const is_marker_hot = (node) =>
        hovered_marker === node.id ||
        hot_edges.some((edge) => edge.sources[0] === node.id || edge.targets[0] === node.id);

    setContext("viz", {
        ...viz,
        hover: {
            ...viz.hover,
            is_focus: (data) => viz.hover.is_focus(data) || ends.has(data.id),
            is_marker_hot,
            enter_marker: (node) => (hovered_marker = node.id),
            leave_marker: () => (hovered_marker = null),
        },
    });

    const width = $derived(pane.layout.width ?? 0);
    const height = $derived(pane.layout.height ?? 0);
</script>

<svg {width} {height} viewBox={`0 0 ${width} ${height}`}>
    <defs>
        <ArrowMarkers />

        <clipPath id="{uid}-clip">
            <rect {width} {height} rx="6" />
        </clipPath>
    </defs>

    <rect
        class="canvas"
        x="0.5"
        y="0.5"
        width={Math.max(0, width - 1)}
        height={Math.max(0, height - 1)}
        rx="5.5"
    />

    <g clip-path="url(#{uid}-clip)">
        {#each containers as node (node.id)}
            <StateNode {node} outer_id={pane.id} />
        {/each}

        <!-- edges are drawn over the containers -->
        {#each edges as edge (edge.id)}
            <Edge
                {edge}
                onenter={() => (hovered = edge.id)}
                onleave={() => (hovered = null)}
            />
        {/each}

        <!-- the highlighted edges are drawn again on top, so reordering never disturbs the hover -->
        {#each hot_edges as edge (edge.id)}
            <Edge {edge} hot />
        {/each}

        {#each leaves as node (node.id)}
            <StateNode {node} outer_id={pane.id} />
        {/each}
    </g>
</svg>

<style>
    svg {
        flex: none;

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
</style>
