<!--
@component
The small expand/collapse icon at the right of a state's name: a box with a plus (collapsed) or a
minus (expanded). Drawn in the diagram's SVG.
-->

<script>
    // right: x of the right edge of the state that owns the icon
    let { right, y, plus, ontoggle } = $props();
</script>

<!-- a box with a minus (open) or a plus (closed) -->
<g class="toggle" role="presentation" onclick={ontoggle}>
    <rect x={right - 16} {y} width="10" height="10" rx="2" />
    <line x1={right - 14} x2={right - 8} y1={y + 5} y2={y + 5} />
    {#if plus}
        <line x1={right - 11} x2={right - 11} y1={y + 2} y2={y + 8} />
    {/if}
</g>
