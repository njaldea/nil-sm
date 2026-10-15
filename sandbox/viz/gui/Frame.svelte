<!--
@component
The page of the viewer, mounted by viewer.hpp as its "index" frame: it loads the machine's
barriers and root props from the server and shows the diagram (Model.svelte) of the root or of a
chosen barrier. A right click on the diagram opens a menu with the settings: theme, whether to
list props, the speed of expanding and collapsing, and which model to show.
-->

<script>
    import { tick } from "svelte";
    import "https://unpkg.com/elkjs@0.12.0/lib/elk.bundled.js";
    import Model from "./Model.svelte";

    // light or dark; remembered between visits, first visit follows the system setting
    const stored_theme =
        typeof localStorage !== "undefined" ? localStorage.getItem("viz-theme") : null;
    let theme = $state(
        stored_theme ??
            (window.matchMedia?.("(prefers-color-scheme: dark)").matches ? "dark" : "light")
    );

    $effect(() => {
        localStorage.setItem("viz-theme", theme);
        document.body.style.background = theme === "dark" ? "#0a0a0a" : "";
    });

    // the options menu, opened at the pointer by a right click on the diagram
    let options_open = $state(false);
    let menu = $state(null);
    let menu_x = $state(0);
    let menu_y = $state(0);

    const inside_diagram = (event) => event.target.closest?.("svg") != null;

    // Only a right click that nothing in the diagram handled opens the menu.
    const open_menu = async (event) => {
        if (event.defaultPrevented || !inside_diagram(event)) {
            return;
        }
        event.preventDefault();
        menu_x = event.clientX;
        menu_y = event.clientY;
        options_open = true;

        // keep the menu inside the window
        await tick();
        const size = menu.getBoundingClientRect();
        menu_x = Math.max(8, Math.min(menu_x, window.innerWidth - size.width - 8));
        menu_y = Math.max(8, Math.min(menu_y, window.innerHeight - size.height - 8));
    };

    // index into the focus options of the current machine
    let selected = $state(0);

    let show_props = $state(false);

    // how fast a state expands or collapses, as a multiple of the default
    let zoom_speed = $state(1);

    // the barriers and root props; the states themselves are fetched by the model
    let machine = $state(null);
    let failure = $state(null);

    fetch("/model")
        .then((response) => response.json())
        .then((body) => {
            machine = body;
        })
        .catch((exception) => {
            failure = String(exception);
        });

    // the root graph, then one entry per barrier
    const options = $derived(
        machine
            ? [
                  { label: "root", focus: null },
                  ...(machine.barriers ?? []).map((barrier) => ({
                      label: barrier.name,
                      focus: barrier.id,
                  })),
              ]
            : []
    );

    const current = $derived(options[selected] ?? options[0]);
</script>

<svelte:window
    onclick={(event) => {
        if (options_open && !menu?.contains(event.target)) {
            options_open = false;
        }
    }}
    oncontextmenu={(event) => {
        if (options_open && !menu?.contains(event.target) && !inside_diagram(event)) {
            options_open = false;
        }
    }}
    onkeydown={(event) => {
        if (event.key === "Escape") {
            options_open = false;
        }
    }}
/>

<div class="frame" class:dark={theme === "dark"}>
    {#if options_open}
        <div class="panel" bind:this={menu} style:left="{menu_x}px" style:top="{menu_y}px">
            <label>
                theme

                <select bind:value={theme}>
                    <option value="light">light</option>
                    <option value="dark">dark</option>
                </select>
            </label>

            {#if options.length > 0}
                <label>
                    props

                    <input type="checkbox" bind:checked={show_props} />
                </label>

                <label>
                    zoom speed

                    <input
                        type="range"
                        min="0.25"
                        max="3"
                        step="0.25"
                        bind:value={zoom_speed}
                    />

                    <span class="value">{zoom_speed.toFixed(2)}x</span>
                </label>

                <label>
                    model

                    <select bind:value={selected}>
                        {#each options as option, index}
                            <option value={index}>{option.label}</option>
                        {/each}
                    </select>
                </label>
            {/if}
        </div>
    {/if}

    {#if machine && current}
        <!-- a fresh Model per selection drops the opened states -->
        {#key selected}
            <div role="presentation" oncontextmenu={open_menu}>
                <Model
                    root_props={machine.root_props ?? []}
                    focus={current.focus}
                    {theme}
                    {show_props}
                    {zoom_speed}
                />
            </div>
        {/key}
    {:else if failure}
        <pre class="waiting">{failure}</pre>
    {:else}
        <p class="waiting">waiting for model...</p>
    {/if}
</div>

<style>
    .frame {
        --text: #1e293b;
        --text-muted: #64748b;
        --control-border: #cbd5e1;
        --control-bg: #ffffff;
        color-scheme: light;
        color: var(--text);
        display: flex;
        flex-direction: column;
        gap: 8px;
        align-items: flex-start;
        padding: 16px 20px;
    }

    .frame.dark {
        --text: #e5e5e5;
        --text-muted: #a3a3a3;
        --control-border: #404040;
        --control-bg: #1f1f1f;
        color-scheme: dark;
    }

    label {
        display: flex;
        align-items: center;
        gap: 6px;
        color: var(--text-muted);
        font-size: 12px;
        font-weight: 500;
    }

    .panel {
        position: fixed;
        z-index: 10;
        display: flex;
        flex-direction: column;
        gap: 8px;
        min-width: 240px;
        padding: 10px 16px 10px 12px;
        border: 1px solid var(--control-border);
        border-radius: 6px;
        background: var(--control-bg);
        box-shadow: 0 4px 14px rgb(0 0 0 / 0.15);
    }

    .panel label {
        justify-content: space-between;
        gap: 12px;
    }

    /* a fixed width, so the slider does not move as the number changes */
    .panel input[type="range"] {
        width: 110px;
    }

    .value {
        min-width: 5ch;
        text-align: right;
        font-variant-numeric: tabular-nums;
    }

    select {
        padding: 2px 4px;
        border: 1px solid var(--control-border);
        border-radius: 4px;
        background: var(--control-bg);
        color: var(--text);
        font-size: 11px;
    }

    .waiting {
        color: var(--text-muted);

        font-size: 12px;
        font-style: italic;
    }
</style>
