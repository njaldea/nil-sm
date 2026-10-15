/*
 * Routes the transitions between placed states as orthogonal lines. ELK places the states;
 * `route_edges` then picks the route of every edge itself and places the edge labels.
 */
import { NODE, ROUTE } from "./constants.js";

const { START_SIZE } = NODE;
const { LABEL_GAP, LEG, CLEARANCE, PORT_GAP } = ROUTE;

/*
 * Re-route every edge once the nodes are placed. Edges leave a state from its bottom, left or
 * right, never the top, and enter a state only from its top; a termination takes any side.
 *   1. a straight line down when the nodes overlap horizontally,
 *   2. (termination only) a straight line sideways, or an L that enters its side,
 *   3. an L that leaves from the side and enters the top of the target,
 *   4. a Z that leaves the bottom and enters the top,
 *   5. a loop that leaves from the side and drops into the top from above,
 *   6. otherwise the route ELK found.
 * Candidates must not cross other nodes. Labels only avoid other labels and nodes.
 * Edges sharing a side of a node are spread evenly along that side.
 */
const rect_of = (node) => ({
    end: node.kind === "end",
    x: node.x,
    y: node.y,
    w: node.width,
    h: node.kind === "start" ? START_SIZE : node.height,
});

const rects_hit = (a, b, pad) =>
    a.x < b.x + b.w + pad &&
    a.x + a.w + pad > b.x &&
    a.y < b.y + b.h + pad &&
    a.y + a.h + pad > b.y;

const clamp = (value, low, high) =>
    Math.min(Math.max(value, low), Math.max(low, high));

// a straight line follows whichever end has a position of its own
const mean_of = (a, b) => (a == null ? b : b == null ? a : (a + b) / 2);

/*
 * Each option names the side it leaves (a_side) and enters (b_side). build(a, b)
 * draws it with a = position along the leaving side and b = position along the
 * entering side, or null when those positions no longer fit the shape.
 */
const route_options = (s, t) => {
    const out = [];
    const s_right = s.x + s.w;
    const s_bottom = s.y + s.h;
    const t_right = t.x + t.w;
    const t_bottom = t.y + t.h;
    const below = t.y >= s_bottom;
    const right_of = t.x >= s_right;
    const left_of = t_right <= s.x;

    const x_low = Math.max(s.x, t.x);
    const x_high = Math.min(s_right, t_right);
    const y_low = Math.max(s.y, t.y);
    const y_high = Math.min(s_bottom, t_bottom);
    const min_overlap = Math.min(10, s.w, t.w);
    // only a termination may be entered from the side or the bottom
    const any_side = t.end;

    // 1. straight down
    if (below && x_high - x_low >= min_overlap) {
        const centre = (x_low + x_high) / 2;
        out.push({
            straight: true,
            a_side: "bottom",
            b_side: "top",
            a0: centre,
            b0: centre,
            a_range: [x_low + 3, x_high - 3],
            build: (a, b) => {
                const x = clamp(mean_of(a, b), x_low + 3, x_high - 3);
                return [{ x, y: s_bottom }, { x, y: t.y }];
            },
        });
    }
    if (any_side && (right_of || left_of) && y_high - y_low >= Math.min(10, s.h, t.h)) {
        const centre = (y_low + y_high) / 2;
        out.push({
            straight: true,
            a_side: right_of ? "right" : "left",
            b_side: right_of ? "left" : "right",
            a0: centre,
            b0: centre,
            a_range: [y_low + 3, y_high - 3],
            build: (a, b) => {
                const y = clamp(mean_of(a, b), y_low + 3, y_high - 3);
                return right_of
                    ? [{ x: s_right, y }, { x: t.x, y }]
                    : [{ x: s.x, y }, { x: t_right, y }];
            },
        });
    }

    // 3. sideways out of the source, vertical into the top of the target
    // (a termination can also be entered from the bottom)
    if (right_of || left_of) {
        const x0 = right_of ? s_right : s.x;
        const a0 = s.y + s.h / 2;
        const b0 = t.x + t.w / 2;
        if (right_of ? b0 > x0 : b0 < x0) {
            const enters_bottom = (a) => any_side && a >= t.y;
            out.push({
                a_side: right_of ? "right" : "left",
                b_side: enters_bottom(a0) ? "bottom" : "top",
                b_side_for: (a) => (enters_bottom(a) ? "bottom" : "top"),
                a0,
                b0,
                a_range: [s.y + 3, s_bottom - 3],
                b_range: [t.x + 3, t_right - 3],
                build: (a, b) => {
                    const from_top = a <= t.y - LEG;
                    const from_bottom = any_side && a >= t_bottom + LEG;
                    if (
                        (!from_top && !from_bottom) ||
                        (right_of ? b - x0 < LEG : x0 - b < LEG)
                    ) {
                        return null;
                    }
                    return [
                        { x: x0, y: a },
                        { x: b, y: a },
                        { x: b, y: from_top ? t.y : t_bottom },
                    ];
                },
            });
        }
    }

    // 3b. (termination only) down out of the source, horizontal into the left or right side
    if (any_side && below && t.y + t.h / 2 > s_bottom) {
        for (const side of ["left", "right"]) {
            const low = side === "left" ? s.x + 3 : Math.max(s.x + 3, t_right + LEG);
            const high = side === "left" ? Math.min(s_right - 3, t.x - LEG) : s_right - 3;
            if (low > high) {
                continue;
            }
            out.push({
                a_side: "bottom",
                b_side: side,
                a0: (low + high) / 2,
                b0: t.y + t.h / 2,
                a_range: [low, high],
                b_range: [t.y + 3, t_bottom - 3],
                build: (a, b) =>
                    a < low || a > high || b - s_bottom < LEG
                        ? null
                        : [
                              { x: a, y: s_bottom },
                              { x: a, y: b },
                              { x: side === "left" ? t.x : t_right, y: b },
                          ],
            });
        }
    }

    // 4. down out of the source, across, down into the top of the target
    if (below && t.y - s_bottom >= LEG) {
        // Where the run across sits depends on the exit position, so parallel Zs nest
        // instead of sharing one line: the farther an edge heads along, the sooner it turns.
        const usable = Math.max(0, t.y - s_bottom - LEG);
        const level = (a, b) =>
            s_bottom +
            LEG / 2 +
            usable * (b > a ? 1 - clamp((a - s.x) / s.w, 0, 1) : clamp((a - s.x) / s.w, 0, 1));
        out.push({
            a_side: "bottom",
            b_side: "top",
            a0: s.x + s.w / 2,
            b0: t.x + t.w / 2,
            a_range: [s.x + 3, s_right - 3],
            b_range: [t.x + 3, t_right - 3],
            build: (a, b) =>
                Math.abs(a - b) < 0.5
                    ? [{ x: a, y: s_bottom }, { x: a, y: t.y }]
                    : [
                          { x: a, y: s_bottom },
                          { x: a, y: level(a, b) },
                          { x: b, y: level(a, b) },
                          { x: b, y: t.y },
                      ],
        });
    }

    // 5. sideways out of the source, up/down past the target, then into its top
    for (const side of left_of ? ["left", "right"] : ["right", "left"]) {
        const going_right = side === "right";
        const beyond = going_right ? right_of : left_of;
        const gap = going_right ? t.x - s_right : s.x - t_right;
        if (beyond && gap < 2 * LEG) {
            continue;
        }
        const x0 = going_right ? s_right : s.x;
        // The vertical leg runs in the gap, or outside both nodes (at a few distances), and
        // the run along the target sits at one of two heights, so loops that would share a
        // line can part.
        for (const extra of beyond ? [0] : [0, PORT_GAP, 2 * PORT_GAP]) {
            for (const drop of [LEG, LEG * 0.6]) {
                const yt = t.y - drop;
                // outside the gap, the run along the target must not cut the source
                const cuts_source =
                    !beyond && yt > s.y - CLEARANCE && yt < s_bottom + CLEARANCE;
                const xo = beyond
                    ? x0 + (going_right ? gap / 2 : -gap / 2)
                    : going_right
                      ? Math.max(s_right, t_right) + LEG + extra
                      : Math.min(s.x, t.x) - LEG - extra;
                out.push({
                    a_side: side,
                    b_side: "top",
                    a0: s.y + s.h / 2,
                    b0: t.x + t.w / 2,
                    a_range: [s.y + 3, s_bottom - 3],
                    b_range: [t.x + 3, t_right - 3],
                    build: (a, b) =>
                        cuts_source
                            ? null
                            : [
                                  { x: x0, y: a },
                                  { x: xo, y: a },
                                  { x: xo, y: yt },
                                  { x: b, y: yt },
                                  { x: b, y: t.y },
                              ],
                });
            }
        }
    }

    return out;
};

const place_label = (label, points, nodes, taken) => {
    const w = label.width ?? 0;
    const h = label.height ?? 0;

    const spots = [];
    const segments = points
        .slice(1)
        .map((q, i) => [points[i], q])
        .sort(
            (a, b) =>
                Math.hypot(b[1].x - b[0].x, b[1].y - b[0].y) -
                Math.hypot(a[1].x - a[0].x, a[1].y - a[0].y)
        );

    for (const [p, q] of segments) {
        for (const at of [0.5, 0.25, 0.75]) {
            const mx = p.x + (q.x - p.x) * at;
            const my = p.y + (q.y - p.y) * at;
            // slide along the segment, then move further away from it
            for (const across of [0, 1, 2]) {
                for (const along of [0, -1, 1, -2, 2]) {
                    if (p.x === q.x) {
                        const dy = along * (h + 2);
                        const dx = across * (w + 2);
                        spots.push({ x: mx + LABEL_GAP + dx, y: my - h / 2 + dy });
                        spots.push({ x: mx - LABEL_GAP - w - dx, y: my - h / 2 + dy });
                    } else {
                        const dx = along * (w + 2);
                        const dy = across * (h + 2);
                        spots.push({ x: mx - w / 2 + dx, y: my - h - LABEL_GAP / 2 - dy });
                        spots.push({ x: mx - w / 2 + dx, y: my + LABEL_GAP / 2 + dy });
                    }
                }
            }
        }
    }

    const free_of_labels = (spot) =>
        !taken.some((other) =>
            rects_hit({ x: spot.x, y: spot.y, w, h }, other, 2)
        );
    const free_of_nodes = (spot) =>
        !nodes.some((node) =>
            rects_hit({ x: spot.x, y: spot.y, w, h }, node, 0)
        );

    return (
        spots.find((s) => free_of_labels(s) && free_of_nodes(s)) ??
        spots.find(free_of_labels) ??
        spots[0] ?? { x: label.x ?? 0, y: label.y ?? 0 }
    );
};

const permutations = (items) =>
    items.length <= 1
        ? [items]
        : items.flatMap((item, i) =>
              permutations([...items.slice(0, i), ...items.slice(i + 1)]).map((rest) => [
                  item,
                  ...rest,
              ])
          );

// Crossings between different edges count fully; running along one another counts half.
const crossing_cost = (routes) => {
    const segments = routes.map((points) =>
        points.slice(1).map((q, i) => [points[i], q])
    );
    const horizontal = (segment) => Math.abs(segment[0].y - segment[1].y) < 0.01;
    let cost = 0;
    for (let i = 0; i < segments.length; i += 1) {
        for (let j = i + 1; j < segments.length; j += 1) {
            for (const a of segments[i]) {
                for (const b of segments[j]) {
                    if (horizontal(a) !== horizontal(b)) {
                        const [h, v] = horizontal(a) ? [a, b] : [b, a];
                        const left = Math.min(h[0].x, h[1].x);
                        const right = Math.max(h[0].x, h[1].x);
                        const top = Math.min(v[0].y, v[1].y);
                        const bottom = Math.max(v[0].y, v[1].y);
                        if (
                            v[0].x > left + 0.5 &&
                            v[0].x < right - 0.5 &&
                            h[0].y > top + 0.5 &&
                            h[0].y < bottom - 0.5
                        ) {
                            cost += 1;
                        }
                    } else {
                        const along = horizontal(a) ? "x" : "y";
                        const across = horizontal(a) ? "y" : "x";
                        if (Math.abs(a[0][across] - b[0][across]) < 2) {
                            const overlap =
                                Math.min(
                                    Math.max(a[0][along], a[1][along]),
                                    Math.max(b[0][along], b[1][along])
                                ) -
                                Math.max(
                                    Math.min(a[0][along], a[1][along]),
                                    Math.min(b[0][along], b[1][along])
                                );
                            if (overlap > 3) {
                                cost += 0.5;
                            }
                        }
                    }
                }
            }
        }
    }
    return cost;
};

export const route_edges = (children, edges) => {
    const by_id = new Map(children.map((c) => [c.id, c]));
    const rects = children.map((c) => ({ id: c.id, ...rect_of(c) }));
    const taken = [];

    const clear = (points, skip, pad = 1) =>
        points.slice(1).every((q, i) => {
            const p = points[i];
            const seg = {
                x: Math.min(p.x, q.x),
                y: Math.min(p.y, q.y),
                w: Math.abs(p.x - q.x),
                h: Math.abs(p.y - q.y),
            };
            return !rects.some(
                (r) => !skip.includes(r.id) && rects_hit(seg, r, pad)
            );
        });

    // Pass 1: every option that is clear with centred ports, best first.
    const plans = edges.map((edge, id) => {
        const s = by_id.get(edge.sources[0]);
        const t = by_id.get(edge.targets[0]);
        if (!s || !t) {
            return { edge, id, options: [] };
        }
        const skip = [s.id, t.id];
        // Slide along the leaving side from the centre until the route is clear.
        const first_clear = (option, pad) => {
            const [low, high] = option.a_range;
            const step = 6;
            const count = Math.ceil((high - low) / step);
            const positions = [option.a0];
            for (let i = 1; i <= count; i += 1) {
                positions.push(option.a0 + i * step, option.a0 - i * step);
            }
            // L routes can also move their entry point along the target side.
            const entries = [option.b0];
            if (!option.straight && option.b_range) {
                const [b_low, b_high] = option.b_range;
                for (let i = 1; i <= Math.ceil((b_high - b_low) / step); i += 1) {
                    entries.push(option.b0 + i * step, option.b0 - i * step);
                }
            }
            for (const a of positions) {
                if (a < low || a > high) {
                    continue;
                }
                for (const entry of entries) {
                    if (
                        option.b_range &&
                        !option.straight &&
                        (entry < option.b_range[0] || entry > option.b_range[1])
                    ) {
                        continue;
                    }
                    const b = option.straight ? a : entry;
                    const points = option.build(a, b);
                    if (points && clear(points, skip, pad)) {
                        return {
                            ...option,
                            pad,
                            a0: a,
                            b0: b,
                            b_side: option.b_side_for?.(a) ?? option.b_side,
                        };
                    }
                }
            }
            return null;
        };
        // Keep a comfortable distance from other nodes when possible, touching distance otherwise.
        const options = route_options(rect_of(s), rect_of(t))
            .map((o) => first_clear(o, CLEARANCE) ?? first_clear(o, 1))
            .filter(Boolean);
        return { edge, id, s, t, skip, options, index: 0, locked: false, a: null, b: null };
    });

    // Pass 2: spread the edges that share a side of a node evenly along it.
    const vertical_side = (side) => side === "top" || side === "bottom";

    // Which way an edge heads once it has left (or before it reaches) a side, and how far it
    // runs before turning. Ordering by this keeps edges on one side from crossing.
    const profile = (points, side, reversed) => {
        const path = reversed ? [...points].reverse() : points;
        if (path.length <= 2) {
            return { dir: 0, d: 0 };
        }
        const first = path[0];
        const last = path[path.length - 1];
        return vertical_side(side)
            ? { dir: Math.sign(last.x - first.x), d: Math.abs(path[1].y - first.y) }
            : { dir: Math.sign(last.y - first.y), d: Math.abs(path[1].x - first.x) };
    };
    // Along a side: edges heading the low way first (turning soonest first), then straight ones,
    // then edges heading the high way (turning furthest first).
    const order = (p, q) => {
        const rank = (item) => (item.dir < 0 ? 0 : item.dir === 0 ? 1 : 2);
        if (rank(p) !== rank(q)) {
            return rank(p) - rank(q);
        }
        if (p.dir < 0) {
            return p.d - q.d;
        }
        return p.dir > 0 ? q.d - p.d : p.key - q.key;
    };

    // Orders chosen by the crossing search below; they win over the heuristic order.
    const forced = new Map();
    let last_groups = new Map();

    const assign_slots = () => {
        const groups = new Map();
        last_groups = groups;
        const join = (node, side, plan, role, other) => {
            const key = `${node.id}:${side}`;
            if (!groups.has(key)) {
                groups.set(key, { node, side, items: [] });
            }
            const vertical_side = side === "top" || side === "bottom";
            const option = plan.options[plan.index];
            const points = option.build(option.a0, option.b0);
            const { dir, d } = points
                ? profile(points, side, role === "b")
                : { dir: 0, d: 0 };
            groups.get(key).items.push({
                plan,
                role,
                dir,
                d,
                key: vertical_side
                    ? other.x + other.width / 2
                    : other.y + other.height / 2,
            });
        };
        for (const plan of plans) {
            plan.a = null;
            plan.b = null;
            plan.slotted = false;
            const option = plan.options[plan.index];
            if (option) {
                join(plan.s, option.a_side, plan, "a", plan.t);
                join(plan.t, option.b_side, plan, "b", plan.s);
            }
        }
        for (const { node, side, items } of groups.values()) {
            const vertical_side = side === "top" || side === "bottom";
            const start = vertical_side ? node.x : node.y;
            const size = vertical_side ? node.width : node.height;
            const inset = size < 40 ? 3 : Math.min(6, size / 4);
            const low = start + inset;
            const span = size - inset * 2;
            const chosen_order = forced.get(`${node.id}:${side}`);
            if (chosen_order) {
                const rank = (item) => chosen_order.indexOf(`${item.plan.id}:${item.role}`);
                items.sort((p, q) => rank(p) - rank(q));
            } else {
                items.sort(order);
            }
            items.forEach(({ plan, role }, i) => {
                plan[role] = low + (span * (i + 1)) / (items.length + 1);
                plan[`${role}_n`] = items.length;
                if (items.length > 1) {
                    plan.slotted = true;
                }
            });
        }
        for (const plan of plans) {
            const option = plan.options[plan.index];
            if (!option?.straight || (plan.a_n !== 1 && plan.b_n !== 1)) {
                continue;
            }
            // A line with an end of its own enters it at the middle. If the other end's
            // even slot already falls in the middle half of that lone end, use the slot,
            // so shared sides keep their even spacing.
            const lone_is_target = plan.b_n === 1;
            const lone = lone_is_target ? plan.t : plan.s;
            const lone_side = lone_is_target ? option.b_side : option.a_side;
            const slot = lone_is_target ? plan.a : plan.b;
            const across = vertical_side(lone_side);
            const centre = across ? lone.x + lone.width / 2 : lone.y + lone.height / 2;
            const half = (across ? lone.width : lone.height) / 4;
            const shared = lone_is_target ? plan.a_n > 1 : plan.b_n > 1;
            if (shared && slot !== null && Math.abs(slot - centre) <= half) {
                plan.a = lone_is_target ? slot : null;
                plan.b = lone_is_target ? null : slot;
            } else {
                plan.a = option.a0;
                plan.b = null;
                plan.slotted = false;
            }
        }
    };

    // A straight line has one position for both ends. It follows whichever end is shared
    // with other edges; when both are shared and their even slots disagree, enter the
    // target from another side, but only a side nothing else uses, so entries never crowd.
    const MAX_SLOT_GAP = 6;
    const side_in_use = (plan, side) =>
        plans.some((other) => {
            const used_option = other !== plan && other.options[other.index];
            return (
                used_option &&
                ((other.s === plan.t && used_option.a_side === side) ||
                    (other.t === plan.t && used_option.b_side === side))
            );
        });
    for (let round = 0; round < 6; round += 1) {
        assign_slots();
        let changed = false;
        for (const plan of plans) {
            const option = plan.options[plan.index];
            if (!option?.straight) {
                continue;
            }
            if (plan.locked || plan.a === null || plan.b === null) {
                continue;
            }
            if (Math.abs(plan.a - plan.b) <= MAX_SLOT_GAP) {
                continue;
            }
            const next = plan.options.findIndex(
                (o, i) =>
                    i > plan.index &&
                    !o.straight &&
                    o.a_side === option.a_side &&
                    o.b_side === option.b_side
            );
            const other_side = plan.options.findIndex(
                (o, i) =>
                    i > plan.index &&
                    !o.straight &&
                    !side_in_use(plan, o.b_side)
            );
            // A termination is small, so a second entry goes to another side, not beside the first.
            const demote_to = plan.t.kind === "end"
                ? other_side >= 0
                    ? other_side
                    : next
                : next >= 0
                  ? next
                  : other_side;
            if (demote_to >= 0) {
                plan.index = demote_to;
                changed = true;
            } else {
                plan.locked = true;
            }
        }
        if (!changed) {
            break;
        }
        if (round === 5) {
            assign_slots();
        }
    }

    // Positions already taken on each side of a node, so parallel edges never share a line.
    const taken_ports = new Map();
    const ports_of = (plan, option, points) => [
        [
            `${plan.s.id}:${option.a_side}`,
            vertical_side(option.a_side) ? points[0].x : points[0].y,
            vertical_side(option.a_side) ? plan.s.width : plan.s.height,
        ],
        [
            `${plan.t.id}:${option.b_side}`,
            vertical_side(option.b_side)
                ? points[points.length - 1].x
                : points[points.length - 1].y,
            vertical_side(option.b_side) ? plan.t.width : plan.t.height,
        ],
    ];
    // A small node (a termination) is too small for ports a full PORT_GAP apart; its
    // ports keep to the even spacing of their slots instead.
    const crowded = (ports) =>
        ports.some(([key, value, size]) =>
            (taken_ports.get(key) ?? []).some(
                (other) => size >= 40 && Math.abs(other - value) < PORT_GAP
            )
        );

    const route_for = (plan) => {
        const option = plan.options[plan.index];
        if (!option) {
            return null;
        }
        const { a, b } = plan;
        const [low, high] = option.a_range;
        const desired = a ?? option.a0;

        const attempts = [
            option.straight && (a !== null || b !== null)
                ? option.build(a, b)
                : option.build(a ?? option.a0, b ?? option.b0),
        ];
        const count = Math.ceil((high - low) / 6);
        for (let i = 1; i <= count; i += 1) {
            for (const x of [desired + i * 6, desired - i * 6]) {
                if (x >= low && x <= high) {
                    attempts.push(
                        option.straight
                            ? option.build(x, x)
                            : option.build(x, b ?? option.b0)
                    );
                }
            }
        }

        // A slot from the even spacing is already unique, so it skips the crowding check;
        // only slid-along fallbacks must keep clear of positions taken earlier.
        const chosen =
            attempts.find(
                (points, index) =>
                    points &&
                    clear(points, plan.skip, option.pad) &&
                    ((index === 0 && plan.slotted) ||
                        !crowded(ports_of(plan, option, points)))
            ) ??
            [option.build(option.a0, option.b0)].find(
                (points) => points && clear(points, plan.skip, option.pad)
            ) ??
            null;

        if (chosen) {
            for (const [key, value] of ports_of(plan, option, chosen)) {
                taken_ports.set(key, [...(taken_ports.get(key) ?? []), value]);
            }
        }
        return chosen;
    };

    // Edges that fit their even slots are placed first, so edges that must slide along a
    // side keep clear of them instead of the other way round.
    const route_all = () => {
        taken_ports.clear();
        const routed = new Map();
        for (const plan of plans) {
            const option = plan.options[plan.index];
            if (!option || !plan.slotted) {
                continue;
            }
            const points =
                option.straight && (plan.a !== null || plan.b !== null)
                    ? option.build(plan.a, plan.b)
                    : option.build(plan.a ?? option.a0, plan.b ?? option.b0);
            if (points && clear(points, plan.skip, option.pad)) {
                routed.set(plan, points);
                for (const [key, value] of ports_of(plan, option, points)) {
                    taken_ports.set(key, [...(taken_ports.get(key) ?? []), value]);
                }
            }
        }
        for (const plan of plans) {
            if (!routed.has(plan)) {
                routed.set(plan, route_for(plan));
            }
        }
        return routed;
    };

    // Try every order of the (few) edges sharing a side and keep the one with fewest crossings.
    const route_points = (plan, route) => {
        if (route) {
            return route;
        }
        const section = plan.edge.sections?.[0];
        return section
            ? [section.startPoint, ...(section.bendPoints ?? []), section.endPoint]
            : [];
    };
    // Crossings are still the primary concern, but route quality needs to be visible in
    // the score too. Previously a long multi-bend detour was almost free (BEND_COST was
    // only 0.01), which made the optimizer prefer geometrically ugly routes whenever they
    // removed a crossing.
    const CROSSING_COST = 100;
    const BEND_COST = 1.5;
    const LENGTH_COST = 0.035;
    const DETOUR_COST = 0.08;
    const SIDE_ENTRY_COST = 1.0;
    const PORT_SKEW_COST = 0.02;
    let crossings = 0;
    let longest_route = 0;

    const route_length = (points) =>
        points.slice(1).reduce(
            (sum, q, i) =>
                sum +
                Math.abs(q.x - points[i].x) +
                Math.abs(q.y - points[i].y),
            0
        );

    // A route should not wander much farther than the Manhattan distance between its
    // endpoints. A small unavoidable detour is fine; large detours are visually noisy.
    const route_detour = (points) => {
        if (points.length < 2) {
            return 0;
        }
        const direct =
            Math.abs(points.at(-1).x - points[0].x) +
            Math.abs(points.at(-1).y - points[0].y);
        if (direct < 1) {
            return route_length(points);
        }
        return Math.max(0, route_length(points) - direct);
    };

    const evaluate = () => {
        assign_slots();
        const routed = route_all();
        const routes = plans.map((plan) => route_points(plan, routed.get(plan)));
        crossings = crossing_cost(routes);
        longest_route = Math.max(0, ...routes.map((route) => route.length));

        const bends = routes.reduce(
            (sum, route) => sum + Math.max(0, route.length - 2),
            0
        );
        const length = routes.reduce((sum, route) => sum + route_length(route), 0);
        const detour = routes.reduce((sum, route) => sum + route_detour(route), 0);

        const side_entries = plans.filter((plan) => {
            const option = plan.options[plan.index];
            return option && plan.t.kind === "end" && option.b_side !== "top";
        }).length;

        // Penalize ports that are forced far from the natural centre. This is deliberately
        // weak: clean crossings and short routes matter more than perfectly centred ports.
        const port_skew = plans.reduce((sum, plan) => {
            const option = plan.options[plan.index];
            if (!option) {
                return sum;
            }
            const a = plan.a ?? option.a0;
            const b = plan.b ?? option.b0;
            return sum +
                Math.abs(a - option.a0) +
                (option.straight ? Math.abs(b - option.b0) : 0);
        }, 0);

        return (
            CROSSING_COST * crossings +
            LENGTH_COST * length +
            DETOUR_COST * detour +
            BEND_COST * bends +
            SIDE_ENTRY_COST * side_entries +
            PORT_SKEW_COST * port_skew
        );
    };
    let best = evaluate();
    // An edge may take a different route if that removes crossings or bends. Later rounds
    // let an edge go back to a simpler route once its neighbours have moved.
    for (let round = 0; round < 3 && plans.length <= 60; round += 1) {
        let moved = false;
        for (const plan of plans) {
            if (plan.options.length < 2) {
                continue;
            }
            const original = plan.index;
            let chosen_index = original;
            for (let i = 0; i < plan.options.length; i += 1) {
                if (i === original) {
                    continue;
                }
                plan.index = i;
                const cost = evaluate();
                // on a tie, the earlier (simpler) option wins
                if (cost < best - 1e-9 || (Math.abs(cost - best) <= 1e-9 && i < chosen_index)) {
                    best = cost;
                    chosen_index = i;
                }
            }
            plan.index = chosen_index;
            best = evaluate();
            moved = moved || chosen_index !== original;
        }
        if (!moved) {
            break;
        }
    }
    // Edges into one node depend on each other, so one-at-a-time moves can get stuck on a
    // long detour; try the simple options of such edges together, each with its best order
    // along the shared sides.
    const evaluate_ordered = () => {
        forced.clear();
        let cost = evaluate();
        for (const [key, group] of [...last_groups]) {
            if (crossings === 0) {
                break;
            }
            if (group.items.length < 2 || group.items.length > 3) {
                continue;
            }
            const ids = group.items.map((item) => `${item.plan.id}:${item.role}`);
            let best_order = null;
            let best_cost = cost;
            for (const candidate of permutations(ids)) {
                forced.set(key, candidate);
                const attempt = evaluate();
                if (attempt < best_cost - 1e-9) {
                    best_cost = attempt;
                    best_order = candidate;
                }
            }
            if (best_order) {
                forced.set(key, best_order);
            } else {
                forced.delete(key);
            }
            cost = evaluate();
        }
        return cost;
    };
    if (plans.length <= 30 && (crossings > 0 || longest_route > 4)) {
        const incoming = new Map();
        for (const plan of plans) {
            if (plan.options.length > 0) {
                incoming.set(plan.t.id, [...(incoming.get(plan.t.id) ?? []), plan]);
            }
        }
        for (const members of incoming.values()) {
            if (members.length < 2 || members.length > 4) {
                continue;
            }
            const width = members.length === 2 ? 8 : members.length === 3 ? 6 : 4;
            const limits = members.map((plan) => Math.min(width, plan.options.length));
            let best_combo = members.map((plan) => plan.index);
            const walk = (k, chosen) => {
                if (k === members.length) {
                    members.forEach((plan, at) => {
                        plan.index = chosen[at];
                    });
                    const cost = evaluate_ordered();
                    if (cost < best - 1e-9) {
                        best = cost;
                        best_combo = [...chosen];
                    }
                    return;
                }
                for (let i = 0; i < limits[k]; i += 1) {
                    chosen[k] = i;
                    walk(k + 1, chosen);
                }
            };
            walk(0, []);
            members.forEach((plan, at) => {
                plan.index = best_combo[at];
            });
            best = evaluate_ordered();
        }
    }
    for (let pass = 0; pass < 2 && crossings > 0 && plans.length <= 60; pass += 1) {
        for (const [key, group] of [...last_groups]) {
            if (group.items.length < 2 || group.items.length > 4) {
                continue;
            }
            const ids = group.items.map((item) => `${item.plan.id}:${item.role}`);
            let best_order = ids;
            for (const candidate of permutations(ids)) {
                forced.set(key, candidate);
                const cost = evaluate();
                if (cost < best) {
                    best = cost;
                    best_order = candidate;
                }
            }
            forced.set(key, best_order);
        }
    }
    assign_slots();
    const routed_final = route_all();

    return plans.map((plan) => {
        const { edge } = plan;
        let points = routed_final.get(plan) ?? null;

        const section = edge.sections?.[0];
        if (!points && section) {
            points = [
                section.startPoint,
                ...(section.bendPoints ?? []),
                section.endPoint,
            ];
        }
        if (!points) {
            return edge;
        }

        const labels = (edge.labels ?? []).map((label) => {
            const spot = place_label(label, points, rects, taken);
            taken.push({
                x: spot.x,
                y: spot.y,
                w: label.width ?? 0,
                h: label.height ?? 0,
            });
            return { ...label, x: spot.x, y: spot.y };
        });

        return {
            ...edge,
            sections: [
                {
                    ...(section ?? {}),
                    startPoint: points[0],
                    bendPoints: points.slice(1, -1),
                    endPoint: points[points.length - 1],
                },
            ],
            labels,
        };
    });
};
