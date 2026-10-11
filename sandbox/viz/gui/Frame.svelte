<script>
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

    // index into the focus options of the current machine
    let selected = $state(0);

    let show_props = $state(false);

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

<div class="frame" class:dark={theme === "dark"}>
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
            model

            <select bind:value={selected}>
                {#each options as option, index}
                    <option value={index}>{option.label}</option>
                {/each}
            </select>
        </label>
    {/if}

    {#if machine && current}
        <!-- a fresh Model per selection drops the opened states -->
        {#key selected}
            <Model root_props={machine.root_props ?? []} focus={current.focus} {theme} {show_props} />
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
