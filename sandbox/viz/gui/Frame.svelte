<script>
    import "https://unpkg.com/elkjs@0.12.0/lib/elk.bundled.js";
    import { xit } from "@nil-/xit";
    import Model from "./Model.svelte";

    const { values } = xit();
    const model = values("model");

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

    // index into the machines the server sends
    let machine = $state(0);

    // index into the focus options of the current machine
    let selected = $state(0);

    let show_props = $state(false);

    const payload = $derived(
        $model && $model.length > 0 ? JSON.parse(new TextDecoder().decode($model)) : null
    );
    const machines = $derived(payload?.machines ?? []);
    const ir = $derived(machines[machine]?.model ?? null);

    // the root graph, then one entry per barrier
    const options = $derived(
        ir
            ? [
                  { label: "root", focus: null },
                  ...(ir.barriers ?? []).map((barrier) => ({
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

    {#if machines.length > 0}
        <label>
            machine

            <select bind:value={machine} onchange={() => (selected = 0)}>
                {#each machines as entry, index}
                    <option value={index}>{entry.name}</option>
                {/each}
            </select>
        </label>
    {/if}

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

    {#if ir && current}
        <!-- a fresh Model per selection drops the opened barriers -->
        {#key `${machine}:${selected}`}
            <Model model={ir} root_props={machines[machine]?.root_props ?? []} focus={current.focus} {theme} {show_props} />
        {/key}
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
