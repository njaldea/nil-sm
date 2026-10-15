<!--
@component
One placed node of the layout, drawn in the diagram's SVG: the start and end markers, a region's
frame, a composite state (a box around its regions) or a leaf state (a collapsed or plain state).
A state shows its name, an expand/collapse icon and its rows, highlights under the pointer, and
toggles on click. The focused state also gets a dashed outline around its name.
The sizes and the hover/toggle handlers come from the "viz" context set by Model.svelte.
-->

<script>
    import { getContext } from "svelte";
    import ActionRows from "./ActionRows.svelte";
    import ToggleIcon from "./ToggleIcon.svelte";
    import { has_title } from "./measure.js";

    // node: a placed node of the layout; outer_id: the focused state, which gets the dashed outline
    let { node, outer_id } = $props();

    const { NODE, COMPOSITE, hover, toggleable, toggle } = getContext("viz");
</script>

{#if node.kind === "start"}
    <circle
        class="start"
        cx={node.x + node.width / 2}
        cy={node.y + NODE.START_SIZE / 2}
        r={node.width / 2}
    />
{:else if node.kind === "end"}
    <circle
        class="ring"
        cx={node.x + node.width / 2}
        cy={node.y + node.height / 2}
        r={node.width / 2}
    />

    <circle
        class="end"
        cx={node.x + node.width / 2}
        cy={node.y + node.height / 2}
        r={node.width / 2 - 3}
    />
{:else if node.kind === "region"}
    <!-- a region is a container, not a state -->
    <rect x={node.x} y={node.y} width={node.width} height={node.height} class="region" />

    <text x={node.x + 8} y={node.y + 14} class="region-title">
        Region {node.region_index + 1}
    </text>
{:else if node.composite === true}
    <rect
        x={node.x}
        y={node.y}
        width={node.width}
        height={node.height}
        rx="6"
        role="presentation"
        class="composite"
        class:outer={node.data.id === outer_id}
        class:barrier={node.data.is_barrier}
        class:peer={hover.is_peer(node)}
        class:focus={hover.is_focus(node.data)}
        class:link={toggleable(node.data)}
        onmouseenter={() => hover.enter(node.data)}
        onmouseleave={hover.leave}
        onclick={() => toggle(node.data)}
    />

    {#if node.data.id === outer_id}
        <!-- the same dashed outline the collapsed state has -->
        <rect
            class="inset"
            x={node.x + 3}
            y={node.y + 3}
            width={node.width - 6}
            height={COMPOSITE.TITLE_H - 6}
            rx="3"
        />
    {/if}

    {#if has_title(node.data)}
        <text x={node.x + 10} y={node.y + NODE.H / 2} class="node-text title">
            {node.data.display_name}
        </text>

        <ToggleIcon
            right={node.x + node.width}
            y={node.y + 10}
            plus={false}
            ontoggle={() => toggle(node.data)}
        />
    {/if}

    <ActionRows
        {node}
        y={node.y + (has_title(node.data) ? COMPOSITE.TITLE_H : 0)}
        titled={has_title(node.data)}
        closed
    />
{:else}
    <rect
        x={node.x}
        y={node.y}
        width={node.width}
        height={node.height}
        rx="5"
        role="presentation"
        class:barrier={node.data.is_barrier}
        class:collapsed={node.data.collapsed}
        class:peer={hover.is_peer(node)}
        class:focus={hover.is_focus(node.data)}
        class:link={toggleable(node.data)}
        onmouseenter={() => hover.enter(node.data)}
        onmouseleave={hover.leave}
        onclick={() => toggle(node.data)}
    >
        {#if node.data.collapsed}
            <title>{node.data.region_count} region(s) hidden, click to show</title>
        {/if}
    </rect>

    {#if node.data.collapsed}
        <!-- a second, inner outline marks a state that hides regions -->
        <rect
            class="inset"
            x={node.x + 3}
            y={node.y + 3}
            width={node.width - 6}
            height={NODE.H - 6}
            rx="3"
        />
    {/if}

    {#if toggleable(node.data)}
        <ToggleIcon
            right={node.x + node.width}
            y={node.y + 10}
            plus
            ontoggle={() => toggle(node.data)}
        />
    {/if}

    <text x={node.x + 10} y={node.y + NODE.H / 2} class="node-text title">
        {node.data.display_name}
    </text>

    <ActionRows {node} y={node.y + NODE.H} />
{/if}
