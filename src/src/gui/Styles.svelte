<!--
@component
How everything the diagram draws looks: states, regions, transitions, markers and text. The rules
are global but scoped to `.model-container`, so StateNode, Edge and the other drawing
components share them. The colours are variables defined by that container in Model.svelte.
Render it once, next to the diagram.
-->

<style>
    /* ordinary state */
    :global(.model-container rect) {
        fill: var(--state);

        stroke: var(--stroke);
        stroke-width: 1.25;
    }

    :global(.model-container rect.composite) {
        fill: var(--composite);

        stroke: var(--stroke-strong);
        stroke-width: 1.5;
    }

    /* regions are deliberately visually distinct from states */
    :global(.model-container rect.region) {
        fill: var(--region);

        stroke: var(--stroke-soft);
        stroke-width: 1;

        stroke-dasharray: 4 3;
    }

    /* a state that hides its regions */
    :global(.model-container rect.collapsed) {
        fill: var(--composite);
        stroke: var(--collapsed-stroke);
    }

    /* a barrier is an ordinary state in another colour: a solid outline, plus the dashed inset when collapsed */
    :global(.model-container rect.barrier) {
        fill: var(--barrier-fill);
        stroke: var(--barrier-stroke);
    }

    :global(.model-container rect.inset) {
        fill: none;
        stroke: var(--collapsed-stroke);
        stroke-dasharray: 2 2;
        pointer-events: none;
    }

    :global(.model-container rect.inset.barrier) {
        stroke: var(--barrier-stroke);
    }

    /*
     * What the pointer and the selection do to a state, for every kind of state alike. Each rule
     * outranks the kinds above and the ones before it, so the order is the priority:
     * the other hosts of a hovered barrier < the hovered state < the open (selected) state.
     */
    :global(.model-container rect.state.peer) {
        fill: var(--peer-fill);
        stroke: var(--peer-stroke);
        stroke-width: 2;
    }

    :global(.model-container rect.state.focus) {
        stroke: var(--edge-hot);
        stroke-width: 2;
    }

    :global(.model-container rect.state.opened) {
        stroke: var(--opened-stroke);
        stroke-width: 2;
    }

    :global(.model-container rect.link) {
        cursor: pointer;
    }

    :global(.model-container rect.link:hover) {
        filter: brightness(0.96);
    }

    /* expand/collapse icon */
    :global(.model-container g.toggle) {
        cursor: pointer;
    }

    :global(.model-container g.toggle rect) {
        fill: var(--canvas);
        stroke: var(--stroke);
        stroke-width: 1;
    }

    :global(.model-container g.toggle line) {
        stroke: var(--stroke);
        stroke-width: 1.25;
    }

    /* initial and final pseudo-nodes */
    :global(.model-container circle.start) {
        fill: var(--terminal);
        stroke: none;
    }

    :global(.model-container circle.ring) {
        fill: var(--canvas);

        stroke: var(--terminal);
        stroke-width: 1.5;
    }

    :global(.model-container circle.end) {
        fill: var(--terminal);
        stroke: none;
    }

    :global(.model-container g.marker.hot circle.start),
    :global(.model-container g.marker.hot circle.end) {
        fill: var(--edge-hot);
    }

    :global(.model-container g.marker.hot circle.ring) {
        stroke: var(--edge-hot);
    }

    /* transitions */
    :global(.model-container path) {
        fill: none;

        stroke: var(--edge);
        stroke-width: 1.25;
    }

    :global(.model-container path.arrowhead) {
        fill: var(--edge);
        stroke: none;
    }

    :global(.model-container path.arrowhead.hot) {
        fill: var(--edge-hot);
    }

    /* invisible, wide, so the thin line is easy to hover */
    :global(.model-container path.hit) {
        stroke: transparent;
        stroke-width: 12;
        pointer-events: stroke;
    }

    :global(.model-container path.line) {
        pointer-events: none;
    }

    :global(.model-container g.hot path.line) {
        stroke: var(--edge-hot);
        stroke-width: 2.25;
    }

    :global(.model-container g.hot text.label) {
        fill: var(--edge-hot);
        font-weight: 700;

        /* hides the plain label underneath */
        stroke: var(--canvas);
        stroke-width: 3px;
        paint-order: stroke;
    }

    :global(.model-container g.overlay) {
        pointer-events: none;
    }

    :global(.model-container path.capture) {
        stroke-dasharray: 3 2;
    }

    /* text */
    :global(.model-container text) {
        fill: var(--text);
    }

    /* clicks must reach the rect underneath */
    :global(.model-container text.node-text) {
        pointer-events: none;
        font-weight: 500;
    }

    /* a state's name sits at the same place whether it is collapsed or expanded */
    :global(.model-container text.title) {
        dominant-baseline: central;
    }

    :global(.model-container line.divider) {
        stroke: var(--stroke-soft);
        stroke-width: 1;
        pointer-events: none;
    }

    :global(.model-container text.action) {
        fill: var(--text-muted);

        font-size: 10px;
        dominant-baseline: central;
        pointer-events: none;
    }

    :global(.model-container text.action.missing) {
        fill: var(--missing-text);
    }

    :global(.model-container text.region-title) {
        fill: var(--text-muted);

        font-size: 9px;
        font-weight: 500;

        letter-spacing: 0.03em;
    }

    :global(.model-container text.center) {
        text-anchor: middle;
        dominant-baseline: central;
    }

    :global(.model-container text.label) {
        fill: var(--edge-label);

        font-size: 10px;
        font-weight: 500;
    }
</style>
