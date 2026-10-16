/*
 * Motion of the diagram while a state expands or collapses: the easing, and the scroll that
 * follows the state.
 */

// slow at both ends of the move
export const ease = (p) => (p < 0.5 ? 4 * p ** 3 : 1 - (-2 * p + 2) ** 3 / 2);

export const smoothstep = (t) => t * t * (3 - 2 * t);

// How close the moving box may come to the edge of the window when it is scrolled into view; more
// room is left on the bottom and right, where it is hard to see.
const MARGIN = { near: 16, far: 64 };

/*
 * Builds `follow(progress)`, which scrolls the page along with the box of a state moving inside
 * its view. It returns a promise that resolves once that frame's scroll is applied.
 *
 * While the box expands the page follows its top-left corner, keeping it where it is in the
 * window. While it collapses the page does not move until the box does, then scrolls to where the
 * collapsed box is in view as the box gets there. Either way the scroll stays within what the page
 * can scroll at the start and at the end of the move.
 *
 *   svg      the diagram's element, to find its place on the page
 *   move     { opening, small, big }: the state's box collapsed and expanded, in diagram coordinates
 *   sizes    { before, after }: the { width, height } of the diagram before and after the move
 *   heading  () => 0 | 1: where the move is heading (1: the end it was started towards)
 *   tick     resolves once the diagram has been redrawn for a new progress
 */
export const make_follow = ({ svg, move, sizes, heading, tick }) => {
    const view = svg?.getBoundingClientRect();
    if (!view) {
        return () => Promise.resolve();
    }

    const { opening, small, big } = move;
    const from = opening ? small : big;
    const to = opening ? big : small;

    const page = document.documentElement;
    const origin = { x: view.left + window.scrollX, y: view.top + window.scrollY };

    // the furthest the page can scroll before the move and after it
    const reach = (size, inner, before, after) => ({
        before: Math.max(0, size - inner),
        after: Math.max(0, size - before + after - inner),
    });
    const reach_x = reach(
        page.scrollWidth,
        window.innerWidth,
        sizes.before.width,
        sizes.after.width
    );
    const reach_y = reach(
        page.scrollHeight,
        window.innerHeight,
        sizes.before.height,
        sizes.after.height
    );
    const limit = (scroll, reached, e) =>
        Math.min(Math.max(scroll, 0), reached.before + (reached.after - reached.before) * e);

    // where the page was when the current mode started, and where the box was in the window
    let mode = null;
    let held = null;

    // the scroll that brings a box into the window with the least change from `scroll`
    const reveal = (scroll, near, far, inner) => {
        let revealed = scroll;
        if (far + MARGIN.far > revealed + inner) {
            revealed = far + MARGIN.far - inner;
        }
        if (near - MARGIN.near < revealed) {
            revealed = near - MARGIN.near;
        }
        return revealed;
    };

    return (progress) => {
        // all read now: the frame is applied later, when the move may have been reset
        const e = ease(progress);
        const expanding = opening === (heading() === 1);
        // how far the box has got towards its collapsed end
        const collapsed = opening ? 1 - e : e;

        return tick().then(() => {
            const box = {
                x: origin.x + from.x + (to.x - from.x) * e,
                y: origin.y + from.y + (to.y - from.y) * e,
                w: from.w + (to.w - from.w) * e,
                h: from.h + (to.h - from.h) * e,
            };

            if (expanding !== mode) {
                mode = expanding;
                held = {
                    // expanding: where the corner of the box is in the window
                    corner: { x: box.x - window.scrollX, y: box.y - window.scrollY },
                    // collapsing: the scroll now, how far the box was, and the scroll that shows
                    // the collapsed box
                    scroll: { x: window.scrollX, y: window.scrollY },
                    collapsed,
                    end: {
                        x: reveal(
                            window.scrollX,
                            origin.x + small.x,
                            origin.x + small.x + small.w,
                            window.innerWidth
                        ),
                        y: reveal(
                            window.scrollY,
                            origin.y + small.y,
                            origin.y + small.y + small.h,
                            window.innerHeight
                        ),
                    },
                };
            }

            let left;
            let top;
            if (expanding) {
                left = box.x - held.corner.x;
                top = box.y - held.corner.y;
            } else {
                // The scroll moves to the one that shows the collapsed box as the box gets there.
                const done =
                    held.collapsed >= 1 ? 1 : (collapsed - held.collapsed) / (1 - held.collapsed);
                const t = Math.min(1, Math.max(0, done));
                left = held.scroll.x + (held.end.x - held.scroll.x) * t;
                top = held.scroll.y + (held.end.y - held.scroll.y) * t;
            }

            window.scrollTo({
                left: limit(left, reach_x, e),
                top: limit(top, reach_y, e),
                behavior: "instant",
            });
        });
    };
};
