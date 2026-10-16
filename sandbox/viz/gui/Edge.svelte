<!--
@component
A transition between two states, drawn in the diagram's SVG as a line with an arrowhead and its
labels. `hot` draws the highlighted copy that goes on top of the edge under the pointer.
The arrowheads come from ArrowMarkers.svelte.
-->

<script>
    // hot: the copy drawn on top of the hovered edge
    let { edge, hot = false, onenter, onleave } = $props();

    const path = $derived(
        (edge.sections ?? [])
            .map((section) =>
                [section.startPoint, ...(section.bendPoints ?? []), section.endPoint]
                    .map((point, index) => `${index === 0 ? "M" : "L"}${point.x},${point.y}`)
                    .join(" ")
            )
            .join(" ")
    );
</script>

{#snippet labels()}
    {#each edge.labels ?? [] as label}
        <text x={label.x + label.width / 2} y={label.y + label.height / 2} class="center label">
            {label.text}
        </text>
    {/each}
{/snippet}

{#if hot}
    <g class="edge hot overlay">
        <path d={path} class="line" class:capture={edge.capture} marker-end="url(#arrow-hot)" />

        {@render labels()}
    </g>
{:else}
    <g role="presentation" class="edge" onmouseenter={onenter} onmouseleave={onleave}>
        <!-- wide invisible stroke so the thin line is easy to hover -->
        <path class="hit" d={path} />

        <path d={path} class="line" class:capture={edge.capture} marker-end="url(#arrow)" />

        {@render labels()}
    </g>
{/if}
