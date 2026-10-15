<!--
@component
The arrowheads that transitions end in, as SVG markers: `arrow` for a transition, `arrow-hot` for
the highlighted one. Put it in the diagram's `<defs>`.
-->

<marker
    id="arrow"
    viewBox="0 0 10 10"
    refX="9"
    refY="5"
    markerWidth="5"
    markerHeight="5"
    orient="auto-start-reverse"
>
    <path d="M0,0 L10,5 L0,10 z" class="arrowhead" />
</marker>

<marker
    id="arrow-hot"
    viewBox="0 0 10 10"
    refX="9"
    refY="5"
    markerWidth="5"
    markerHeight="5"
    orient="auto-start-reverse"
>
    <path d="M0,0 L10,5 L0,10 z" class="arrowhead hot" />
</marker>
