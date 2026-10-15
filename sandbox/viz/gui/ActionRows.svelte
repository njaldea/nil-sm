<!--
@component
The rows under a state's name: what it reacts to (hooks, events, captures), the args nothing
provides (in red), and optionally the props it provides. Each block is set off by a divider line.
Drawn inside the diagram's SVG; the sizes come from the "viz" context set by Model.svelte.
-->

<script>
    import { getContext } from "svelte";

    // node: { x, width, data }; y: top of the first row; closed: also draw the line under the last row;
    // titled: a name row sits above, so the first row has a line above it
    let { node, y, closed = false, titled = true } = $props();

    const { ROWS, measure } = getContext("viz");

    const actions = $derived(measure.action_texts(node.data));
    const missing = $derived(measure.missing_texts(node.data));
    const provided = $derived(measure.prop_texts(node.data));

    const missing_top = $derived(y + measure.block_height(actions.length));
    const provided_top = $derived(missing_top + measure.block_height(missing.length));
    const bottom = $derived(y + measure.actions_height(node.data));
</script>

{#if actions.length > 0}
    {#if titled}
        <line class="divider" x1={node.x} x2={node.x + node.width} y1={y} y2={y} />
    {/if}

    {#each actions as line, index}
        <text
            x={node.x + ROWS.PAD_X}
            y={y + ROWS.PAD_Y + ROWS.LINE_H * (index + 0.5)}
            class="action"
        >
            {line}
        </text>
    {/each}
{/if}

{#if missing.length > 0}
    {#if titled || missing_top > y}
        <line class="divider" x1={node.x} x2={node.x + node.width} y1={missing_top} y2={missing_top} />
    {/if}

    {#each missing as line, index}
        <text
            x={node.x + ROWS.PAD_X}
            y={missing_top + ROWS.PAD_Y + ROWS.LINE_H * (index + 0.5)}
            class="action missing"
        >
            {line}
        </text>
    {/each}
{/if}

{#if provided.length > 0}
    {#if titled || provided_top > y}
        <line class="divider" x1={node.x} x2={node.x + node.width} y1={provided_top} y2={provided_top} />
    {/if}

    {#each provided as line, index}
        <text
            x={node.x + ROWS.PAD_X}
            y={provided_top + ROWS.PAD_Y + ROWS.LINE_H * (index + 0.5)}
            class="action"
        >
            {line}
        </text>
    {/each}
{/if}

{#if closed && measure.actions_height(node.data) > 0}
    <line class="divider" x1={node.x} x2={node.x + node.width} y1={bottom} y2={bottom} />
{/if}
