<!--
@component
The box of the state that is expanding or collapsing, drawn once while it moves between its two
sizes. It is drawn in two parts around the state's contents: `box` under them (the fill and
outline) and `chrome` over them (name, icon and rows). It reacts to the pointer like a state, so
it can be clicked to turn the move around.
-->

<script>
    import { getContext } from "svelte";
    import ActionRows from "./ActionRows.svelte";
    import ToggleIcon from "./ToggleIcon.svelte";

    // The focused state's box while it moves between its collapsed and expanded size.
    // part "box" is drawn under the state's contents, part "chrome" over them.
    let { scene, part } = $props();

    const { NODE, hover, toggle } = getContext("viz");

    const box = $derived(scene.box);
    const node = $derived({ x: box.x, y: box.y, width: box.w, height: box.h, data: scene.data });
</script>

{#if part === "box"}
    <rect
        role="presentation"
        class="composite outer link"
        class:barrier={scene.data.is_barrier}
        class:focus={hover.is_focus(scene.data)}
        x={box.x}
        y={box.y}
        width={box.w}
        height={box.h}
        rx={5 + scene.grown}
        style:stroke-width={1.25 + 0.75 * scene.grown}
        onmouseenter={() => hover.enter(scene.data)}
        onmouseleave={hover.leave}
        onclick={() => toggle(scene.data)}
    />
{:else}
    <g class="stale">
        <rect
            class="inset"
            x={box.x + 3}
            y={box.y + 3}
            width={box.w - 6}
            height={NODE.H - 6}
            rx="3"
        />

        <text x={box.x + 10} y={box.y + NODE.H / 2} class="node-text title">
            {scene.data.display_name}
        </text>

        <ToggleIcon right={box.x + box.w} y={box.y + 10} plus={scene.grown < 0.5} />

        <ActionRows {node} y={box.y + NODE.H} closed={scene.grown >= 0.5} />
    </g>
{/if}
