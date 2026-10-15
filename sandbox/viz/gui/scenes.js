import { ZOOM } from "./constants.js";
import { ease, smoothstep } from "./motion.js";

/*
 * What is drawn at a given moment: a list of scenes, each a set of states and edges with an
 * opacity. Besides the current layout, a scene can be the state's box (`kind` "box" or "chrome")
 * or its contents ("inside").
 *
 * Normally there is one scene, the current layout. While a state expands or collapses, `previous`
 * holds the layout being left and the state's box is drawn once, moving between its two sizes:
 * its contents are clipped to it and fade in or out, and everything around it fades the other way.
 *
 *   current   { focus, containers, edges, leaves } of the layout that is shown
 *   previous  the layout being left plus { opening, id, small, big, ... }, or null
 *   progress  how far the move has run, 0 to 1
 */
export const build_scenes = (current, previous, progress) => {
    const live = { key: "current", live: true, ...current, opacity: 1 };
    if (!previous) {
        return [live];
    }

    const eased = ease(progress);
    // the views overlap only in the middle of the move, which keeps the double image short
    const fade = smoothstep(
        Math.min(1, Math.max(0, (progress - ZOOM.FADE_START) / ZOOM.FADE_SPAN))
    );

    const { opening, id, small, big } = previous;
    const small_view = opening ? previous : current;
    const big_view = opening ? current : previous;

    const from = opening ? small : big;
    const to = opening ? big : small;
    const box = {
        x: from.x + (to.x - from.x) * eased,
        y: from.y + (to.y - from.y) * eased,
        w: from.w + (to.w - from.w) * eased,
        h: from.h + (to.h - from.h) * eased,
    };

    // 0 while the box is the collapsed one, 1 once it is the focused one
    const grown = opening ? eased : 1 - eased;
    const reveal = opening ? fade : 1 - fade;
    const data = grown < 0.5 ? small.data : big.data;
    const is_other = (node) => node.data?.id !== id;

    return [
        {
            key: "outside",
            live: false,
            focus: small_view.focus,
            containers: small_view.containers,
            edges: small_view.edges,
            leaves: small_view.leaves.filter(is_other),
            opacity: 1 - reveal,
        },
        { key: "box", kind: "box", box, grown, data },
        {
            key: "inside",
            kind: "inside",
            live: false,
            focus: id,
            containers: big_view.containers.filter(is_other),
            edges: big_view.edges,
            leaves: big_view.leaves,
            opacity: reveal,
            box,
            shift: { x: box.x - big.x, y: box.y - big.y },
        },
        { key: "chrome", kind: "chrome", box, grown, data },
    ];
};
