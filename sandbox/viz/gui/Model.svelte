<script>
    // model: one machine's IR; focus: id of the barrier to show, or null for the root.
    let { model, root_props = [], focus = null, theme = "light", show_props = false } = $props();

    const elk = new window.ELK();

    const TERMINATION = "[**]";

    const CHAR_W = 6.5;
    const TEXT_PAD = 18;

    const NODE_H = 30;
    const START_SIZE = 12;
    const START_GAP = 26;
    const END_SIZE = 26;

    const REGION_PAD_X = 10;
    const REGION_PAD_TOP = 24;
    const REGION_PAD_BOTTOM = 10;
    const REGION_GAP = 10;

    const COMPOSITE_PAD_TOP = 34;
    const COMPOSITE_PAD_X = 10;
    const COMPOSITE_PAD_BOTTOM = 10;

    const text_width = (text = "") =>
        Math.max(50, text.length * CHAR_W + TEXT_PAD);

    const state_width = (node) =>
        Math.max(70, text_width(node.display_name));

    const edge_text_width = (text = "") =>
        Math.max(20, text.length * CHAR_W + 8);

    // Actions are the second row of a state: hooks and event/capture responses.
    const ACTION_CHAR_W = 5.8;
    const ACTION_LINE_H = 13;
    const ACTION_PAD_X = 8;
    const ACTION_PAD_Y = 5;
    const COMPOSITE_TITLE_H = 28;
    // Room at the right of a title for the expand/collapse icon.
    const TOGGLE_W = 18;

    const action_text = (action) => {
        switch (action.type) {
            case "entry":
                return `on_enter / ${action.response}`;
            case "exit":
                return `on_exit / ${action.response}`;
            case "regions_finalized":
                return `on_regions_finalized / ${action.response}`;
            case "capture":
                return `capture ${action.event} / ${action.response}`;
            default:
                return `${action.event} / ${action.response}`;
        }
    };

    // The path starts with the reporting host's own barrier, which the row need not repeat.
    const missing_text = ({ dependency, barrier_path, as_parent }) => {
        const nested = barrier_path.slice(1);
        let name = dependency.type_name;
        if (as_parent) {
            name = `as_parent<${dependency.type_name}>`;
        } else if (dependency.is_direct_parent) {
            name = `direct_parent<${dependency.type_name}>`;
        }
        return `missing ${name}${
            nested.length > 0 ? ` (${nested[0]})` : ""
        }`;
    };

    // Blocks under the name: the actions, the args nothing provides, then (optionally) the props it provides.
    const action_texts = (node) => (node?.actions ?? []).map(action_text);
    const missing_texts = (node) => (node?.unsatisfied_args ?? []).map(missing_text);
    const prop_texts = (node) =>
        show_props
            ? (node?.provided_props ?? []).map((prop) => `provides ${prop.type_name}`)
            : [];

    const block_height = (count) =>
        count === 0
            ? 0
            : count * ACTION_LINE_H + 2 * ACTION_PAD_Y;

    const actions_height = (node) =>
        block_height(action_texts(node).length) +
        block_height(missing_texts(node).length) +
        block_height(prop_texts(node).length);

    const actions_width = (node) =>
        Math.max(
            0,
            ...[...action_texts(node), ...missing_texts(node), ...prop_texts(node)].map(
                (line) =>
                    line.length * ACTION_CHAR_W +
                    2 * ACTION_PAD_X
            )
        );

    let edge_seq = 0;

    const make_edge = (
        source,
        target,
        text = "",
        capture = false
    ) => ({
        id: `e${edge_seq++}`,
        sources: [source],
        targets: [target],
        labels: text
            ? [
                  {
                      text,
                      width: edge_text_width(text),
                      height: 14,
                      layoutOptions: {
                          "elk.edgeLabels.placement": "CENTER",
                      },
                  },
              ]
            : [],
        capture,
    });

    /*
     * Standard state graph.
     */
    const layered_options = {
        "elk.algorithm": "layered",
        "elk.direction": "DOWN",

        "elk.edgeRouting": "ORTHOGONAL",

        "elk.layered.layering.strategy": "NETWORK_SIMPLEX",
        "elk.layered.cycleBreaking.strategy": "DEPTH_FIRST",

        "elk.layered.nodePlacement.strategy": "BRANDES_KOEPF",
        "elk.layered.nodePlacement.favorStraightEdges": "true",
        "elk.layered.nodePlacement.bk.edgeStraightening":
            "IMPROVE_STRAIGHTNESS",

        "elk.layered.spacing.nodeNodeBetweenLayers": "18",
        "elk.spacing.nodeNode": "14",
        "elk.layered.spacing.edgeNodeBetweenLayers": "10",
        "elk.layered.spacing.edgeEdgeBetweenLayers": "6",

        "elk.layered.crossingMinimization.strategy": "LAYER_SWEEP",
        "elk.layered.compaction.postCompaction": true,
        "elk.layered.mergeEdges": false,

        "elk.separateConnectedComponents": false,
    };

    /*
     * [**] is a synthetic termination node.
     * It is never rendered as a normal state.
     */
    const visible_nodes = (nodes = []) =>
        nodes.filter(
            (node) => node.display_name !== TERMINATION
        );

    /*
     * Ordinary state node.
     */
    const build_simple_node = (node) => ({
        id: node.id,
        kind: "state",
        data: node,
        width: Math.max(
            state_width(node) +
                (node.collapsed || node.expandable ? TOGGLE_W : 0),
            actions_width(node)
        ),
        height: NODE_H + actions_height(node),
    });

    /*
     * Build local transitions for a sibling state graph.
     *
     * Example:
     *
     *     ● -> first
     *
     *     last -> ◎
     *
     * [**] itself is not included.
     */
    const build_local_edges = (nodes, scope) => {
        const real = visible_nodes(nodes);
        const ids = new Set(real.map((node) => node.id));

        const edges = [];
        const pseudo_nodes = [];

        /*
         * Initial state.
         */
        const initial = real.find(
            (node) => node.is_initial
        );

        if (initial) {
            const start_id = `${scope}/start`;

            // Extra room below the circle gives the start transition the same
            // length as a labelled one; the circle is drawn at the top of the box.
            pseudo_nodes.push({
                id: start_id,
                kind: "start",
                width: START_SIZE,
                height: START_SIZE + START_GAP,

                layoutOptions: {
                    "elk.layered.layering.layerConstraint":
                        "FIRST_SEPARATE",
                },
            });

            edges.push(
                make_edge(
                    start_id,
                    initial.id
                )
            );
        }

        /*
         * Termination state.
         *
         * Any transition to [**] creates one final pseudo-node.
         */
        const end_id = `${scope}/end`;
        let has_end = false;

        for (const node of real) {
            for (const transition of node.transitions ?? []) {
                const capture =
                    transition.type === "capture";

                if (transition.target === TERMINATION) {
                    has_end = true;

                    edges.push(
                        make_edge(
                            node.id,
                            end_id,
                            transition.event,
                            capture
                        )
                    );

                    continue;
                }

                /*
                 * Normal transition within this graph.
                 */
                if (ids.has(transition.target)) {
                    edges.push(
                        make_edge(
                            node.id,
                            transition.target,
                            transition.event,
                            capture
                        )
                    );
                }
            }
        }

        if (has_end) {
            // No layer constraint: it sits one layer below its deepest source, level with siblings.
            pseudo_nodes.push({
                id: end_id,
                kind: "end",
                width: END_SIZE,
                height: END_SIZE,
            });
        }

        return {
            edges,
            pseudo_nodes,
        };
    };

    /*
     * Recursively lay out one region.
     *
     * A region is itself a graph containing sibling states.
     *
     * Composite state:
     *
     *     state
     *       |
     *       +-- region
     *       |    +-- state
     *       |
     *       +-- region
     *            +-- state
     */
    // Room for the loops that run around the outside of the top-level states.
    const ROOT_PADDING = "[top=20,left=20,bottom=20,right=20]";

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
    const LABEL_GAP = 4;
    // Shortest leg of an L route, so the arrowhead is never the whole segment.
    const LEG = 18;
    // Preferred distance between a route and nodes it is not attached to, and between parallel ports.
    const CLEARANCE = 12;
    const PORT_GAP = 10;
    // Closest a route or label may come to the edge of the region that holds it.
    const EDGE_MARGIN = 6;

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

    const route_edges = (children, edges) => {
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
    const REGION_PADDING = `[top=${REGION_PAD_TOP},left=${REGION_PAD_X},bottom=${REGION_PAD_BOTTOM},right=${REGION_PAD_X}]`;

    const layout_region = async (
        nodes,
        scope,
        padding = ROOT_PADDING
    ) => {
        const real = visible_nodes(nodes);

        const children = [];
        // Composites go to ELK as opaque boxes; their laid-out content is re-attached afterwards.
        const composites = new Map();

        for (const node of real) {
            if ((node.regions?.length ?? 0) === 0) {
                children.push(
                    build_simple_node(node)
                );
            } else {
                const composite =
                    await layout_composite_state(
                        node,
                        scope
                    );

                composites.set(
                    composite.id,
                    composite
                );

                children.push({
                    id: composite.id,
                    width: composite.width,
                    height: composite.height,
                });
            }
        }

        const {
            edges,
            pseudo_nodes,
        } = build_local_edges(
            nodes,
            scope
        );

        children.push(...pseudo_nodes);

        const result = await elk.layout({
            id: `${scope}/graph`,

            layoutOptions: {
                ...layered_options,
                "elk.padding": padding,
            },

            children,
            edges,
        });

        const placed_children = (
            result.children ?? []
        ).map(
            (child) =>
                composites.has(child.id)
                    ? {
                          ...composites.get(child.id),
                          x: child.x,
                          y: child.y,
                      }
                    : child
        );

        const routed = route_edges(
            placed_children,
            result.edges ?? []
        );

        // Routes (and their labels) that run past the region grow it instead of spilling into a neighbour.
        const xs = [];
        const ys = [];
        for (const edge of routed) {
            for (const section of edge.sections ?? []) {
                for (const point of [
                    section.startPoint,
                    ...(section.bendPoints ?? []),
                    section.endPoint,
                ]) {
                    xs.push(point.x);
                    ys.push(point.y);
                }
            }
            for (const label of edge.labels ?? []) {
                xs.push(label.x, label.x + (label.width ?? 0));
                ys.push(label.y, label.y + (label.height ?? 0));
            }
        }

        const dx = Math.max(0, EDGE_MARGIN - Math.min(...xs, Infinity));
        const dy = Math.max(0, EDGE_MARGIN - Math.min(...ys, Infinity));
        const width = Math.max(
            (result.width ?? 0) + dx,
            Math.max(...xs, -Infinity) + dx + EDGE_MARGIN
        );
        const height = Math.max(
            (result.height ?? 0) + dy,
            Math.max(...ys, -Infinity) + dy + EDGE_MARGIN
        );

        return {
            ...result,
            width,
            height,
            children:
                dx === 0 && dy === 0
                    ? placed_children
                    : placed_children.map((child) => ({
                          ...child,
                          x: child.x + dx,
                          y: child.y + dy,
                      })),
            edges:
                dx === 0 && dy === 0
                    ? routed
                    : routed.map((edge) => shift_edge(edge, dx, dy)),
        };
    };

    /*
     * Shift an ELK edge by dx/dy.
     *
     * This is important because ELK edge coordinates are relative
     * to the graph that owns the edge.
     */
    const shift_edge = (
        edge,
        dx,
        dy
    ) => ({
        ...edge,

        sections: (
            edge.sections ?? []
        ).map((section) => ({
            ...section,

            startPoint: section.startPoint
                ? {
                      x:
                          section.startPoint.x +
                          dx,
                      y:
                          section.startPoint.y +
                          dy,
                  }
                : section.startPoint,

            endPoint: section.endPoint
                ? {
                      x:
                          section.endPoint.x +
                          dx,
                      y:
                          section.endPoint.y +
                          dy,
                  }
                : section.endPoint,

            bendPoints: (
                section.bendPoints ?? []
            ).map((point) => ({
                x: point.x + dx,
                y: point.y + dy,
            })),
        })),

        labels: (
            edge.labels ?? []
        ).map((label) => ({
            ...label,
            x:
                (label.x ?? 0) +
                dx,
            y:
                (label.y ?? 0) +
                dy,
        })),
    });

    /*
     * Make sibling regions the same height.
     *
     * Widths stay natural so regions do not carry empty space at the sides.
     * Shorter regions get their content centered vertically; children and
     * edges must move together.
     */
    const normalize_regions = (
        regions
    ) => {
        const max_height = Math.max(
            0,
            ...regions.map(
                (region) =>
                    region.height ?? 0
            )
        );

        return regions.map(
            (region) => {
                const dy =
                    (max_height -
                        (region.height ?? 0)) /
                    2;

                return {
                    ...region,
                    height: max_height,

                    children: (
                        region.children ?? []
                    ).map(
                        (child) => ({
                            ...child,
                            y:
                                (child.y ?? 0) +
                                dy,
                        })
                    ),

                    edges: (
                        region.edges ?? []
                    ).map(
                        (edge) =>
                            shift_edge(
                                edge,
                                0,
                                dy
                            )
                    ),
                };
            }
        );
    };

    /*
     * Layout a composite state.
     *
     * Every region is rendered simultaneously.
     *
     * Regions are siblings and are placed horizontally.
     */
    const layout_composite_state = async (
        node,
        parent_scope
    ) => {
        const regions = [];

        for (
            let index = 0;
            index < node.regions.length;
            index += 1
        ) {
            const region_states =
                node.regions[index] ?? [];

            const region =
                await layout_region(
                    region_states,
                    `${parent_scope}/${node.id}/region-${index}`,
                    REGION_PADDING
                );

            regions.push({
                ...region,

                id: `${node.id}/region-${index}`,

                kind: "region",

                region_index: index,

                data: {
                    index,
                },
            });
        }

        /*
         * Equalize sibling region sizes.
         */
        const normalized =
            normalize_regions(
                regions
            );

        /*
         * Regions are placed side by side by hand. Their contents were
         * already laid out, so they must not go through ELK again.
         */
        let cursor = COMPOSITE_PAD_X;

        // Regions start below the title and actions rows.
        const top =
            COMPOSITE_PAD_TOP +
            actions_height(node);

        const placed = normalized.map(
            (region) => {
                const result = {
                    ...region,
                    x: cursor,
                    y: top,
                };

                cursor +=
                    region.width +
                    REGION_GAP;

                return result;
            }
        );

        const content_width =
            cursor -
            REGION_GAP +
            COMPOSITE_PAD_X;

        // The title is drawn at the top-left, so a short body must still be wide enough for it.
        const width = Math.max(
            content_width,
            text_width(node.display_name) + 10 + TOGGLE_W,
            actions_width(node)
        );

        const shift = (width - content_width) / 2;

        const content_height =
            Math.max(
                0,
                ...placed.map(
                    (region) =>
                        region.height
                )
            );

        return {
            id: node.id,

            kind: "state",

            composite: true,

            data: node,

            width,

            height:
                content_height +
                top +
                COMPOSITE_PAD_BOTTOM,

            /*
             * Children remain relative to this composite state.
             */
            children: placed.map(
                (region) => ({
                    ...region,
                    x: region.x + shift,
                })
            ),
        };
    };

    /*
     * Barrier occurrences are expanded in place: the host state becomes a composite whose
     * single region is a copy of the barrier definition.
     *
     * Definitions share node ids between occurrences, so every copy gets its host's id
     * as a prefix to keep ids unique across the diagram.
     */
    const prefix_tree = (
        nodes,
        prefix
    ) =>
        nodes.map((node) => ({
            ...node,
            id: `${prefix}${node.id}`,
            transitions: (
                node.transitions ?? []
            ).map((transition) => ({
                ...transition,
                target:
                    transition.target ===
                    TERMINATION
                        ? transition.target
                        : `${prefix}${transition.target}`,
            })),
            regions: (
                node.regions ?? []
            ).map((region) =>
                prefix_tree(
                    region,
                    prefix
                )
            ),
        }));

    // Only hosts listed in `expanded` are opened; `stack` keeps a barrier from containing itself.
    // `props` are the props provided above `nodes` (null: do not judge the states). An opened
    // barrier body is judged against its host's ancestors, so each state shows what it lacks.
    const expand_barriers = (
        nodes,
        barriers,
        expanded,
        stack = [],
        props = null,
        parent = null
    ) =>
        nodes.map((node) => {
            const inner = props && [
                ...props,
                ...(node.provided_props ?? []),
            ];
            const regions = (
                node.regions ?? []
            ).map((region) =>
                expand_barriers(
                    region,
                    barriers,
                    expanded,
                    stack,
                    inner,
                    node
                )
            );

            const definition = node.barrier_id
                ? barriers.get(
                      node.barrier_id
                  )
                : undefined;

            if (
                !definition ||
                regions.length > 0 ||
                stack.includes(
                    node.barrier_id
                )
            ) {
                return {
                    ...node,
                    regions,
                };
            }

            if (!expanded.has(node.id)) {
                return {
                    ...node,
                    regions,
                    expandable: true,
                };
            }

            const body = prefix_tree(
                definition.roots,
                `${node.id}::`
            );

            return {
                ...node,
                expandable: true,
                regions: [
                    expand_barriers(
                        props
                            ? annotate_issues(body, barriers, props, null)
                            : body,
                        barriers,
                        expanded,
                        [
                            ...stack,
                            node.barrier_id,
                        ],
                        props,
                        null
                    ),
                ],
            };
        });

    /*
     * Build the complete graph for either:
     *
     *   ir.roots
     *
     * or:
     *
     *   barrier.roots
     */
    // A collapsed state keeps its transitions but shows no regions; barrier hosts open and close
    // through `expanded` instead.
    const collapse_nodes = (nodes, collapsed) =>
        nodes.map((node) => {
            if ((node.regions?.length ?? 0) === 0) {
                return node;
            }

            if (collapsed.has(node.id) && !node.expandable) {
                return {
                    ...node,
                    regions: [],
                    collapsed: true,
                    region_count: node.regions.length,
                };
            }

            return {
                ...node,
                regions: node.regions.map((region) =>
                    collapse_nodes(region, collapsed)
                ),
            };
        });

    // direct_parent<T> needs the immediate parent to expose T; other args look through ancestors.
    const satisfied = (dependency, props, parent) =>
        (dependency.is_direct_parent ? (parent?.provided_props ?? []) : props).some(
            (prop) => prop.type_id === dependency.type_id
        );

    const unique_issues = (issues) => {
        const seen = new Set();
        return issues.filter((issue) => {
            const key = `${issue.dependency.type_id}:${issue.dependency.is_direct_parent}:${issue.as_parent ?? false}:${issue.barrier_path.join(">")}:${issue.state}`;
            if (seen.has(key)) {
                return false;
            }
            seen.add(key);
            return true;
        });
    };

    /*
     * Validation lives here, not in the IR: whether a barrier is satisfied depends on the host
     * it is used at, so it is judged per occurrence.
     *
     * A state's issues are its own args that no ancestor prop provides, plus, for a barrier host,
     * everything the barrier body cannot get from the host's ancestors. A barrier looks props up
     * through its host but never reaches direct_parent.
     */
    const node_issues = (node, barriers, props, parent) => {
        const own = (node.required_args ?? [])
            .filter((dependency) => !satisfied(dependency, props, parent))
            .map((dependency) => ({
                dependency,
                barrier_path: [],
                state: node.display_name,
            }));

        const definition = barriers.get(node.barrier_id);
        const hosted = definition
            ? collect_issues(definition.roots, barriers, props, null).map((issue) => ({
                  ...issue,
                  barrier_path: [definition.name, ...issue.barrier_path],
              }))
            : [];

        return unique_issues([...own, ...hosted]);
    };

    const descendant_props = (node, props) => [
        ...props,
        ...(node.provided_props ?? []),
    ];

    const collect_issues = (nodes, barriers, props, parent) =>
        unique_issues(
            nodes.flatMap((node) => [
                ...node_issues(node, barriers, props, parent),
                ...(node.regions ?? []).flatMap((region) =>
                    collect_issues(region, barriers, descendant_props(node, props), node)
                ),
            ])
        );

    // Parent side of an unmet direct_parent<T>: what this state would have to expose.
    const parent_issues = (node) => {
        const provided = node.provided_props ?? [];
        return unique_issues(
            (node.regions ?? []).flatMap((region) =>
                region.flatMap((child) =>
                    (child.required_args ?? [])
                        .filter(
                            (dependency) =>
                                dependency.is_direct_parent &&
                                !provided.some((prop) => prop.type_id === dependency.type_id)
                        )
                        .map((dependency) => ({
                            dependency,
                            barrier_path: [],
                            state: node.display_name,
                            as_parent: true,
                        }))
                )
            )
        );
    };

    // Attach `unsatisfied_args` to each state of the tree.
    const annotate_issues = (nodes, barriers, props, parent) =>
        nodes.map((node) => ({
            ...node,
            unsatisfied_args: unique_issues([
                ...node_issues(node, barriers, props, parent),
                ...parent_issues(node),
            ]),
            regions: (node.regions ?? []).map((region) =>
                annotate_issues(region, barriers, descendant_props(node, props), node)
            ),
        }));

    const layout_graph = async (
        roots,
        barriers = [],
        expanded = new Set(),
        collapsed = new Set(),
        props = null
    ) => {
        edge_seq = 0;

        return layout_region(
            collapse_nodes(
                expand_barriers(
                    roots,
                    new Map(barriers.map((barrier) => [barrier.id, barrier])),
                    expanded,
                    [],
                    props
                ),
                collapsed
            ),
            "root"
        );
    };

    /*
     * Convert all nested ELK coordinates into SVG/root coordinates.
     *
     * Critical detail:
     *
     *   - child node coordinates are relative to their parent
     *   - edge coordinates are relative to the graph that owns them
     *
     * Therefore edges use the graph offset, not the child offset.
     */
    const flatten_layout = (
        layout
    ) => {
        const nodes = [];
        const edges = [];

        const shift_point = (
            point,
            offset_x,
            offset_y
        ) => ({
            x:
                (point?.x ?? 0) +
                offset_x,

            y:
                (point?.y ?? 0) +
                offset_y,
        });

        const flatten_edge = (
            edge,
            offset_x,
            offset_y
        ) => ({
            ...edge,

            sections: (
                edge.sections ?? []
            ).map(
                (section) => ({
                    ...section,

                    startPoint:
                        shift_point(
                            section.startPoint,
                            offset_x,
                            offset_y
                        ),

                    endPoint:
                        shift_point(
                            section.endPoint,
                            offset_x,
                            offset_y
                        ),

                    bendPoints: (
                        section.bendPoints ??
                        []
                    ).map(
                        (point) =>
                            shift_point(
                                point,
                                offset_x,
                                offset_y
                            )
                    ),
                })
            ),

            labels: (
                edge.labels ?? []
            ).map(
                (label) => ({
                    ...label,

                    x:
                        (label.x ?? 0) +
                        offset_x,

                    y:
                        (label.y ?? 0) +
                        offset_y,
                })
            ),
        });

        const walk = (
            graph,
            offset_x = 0,
            offset_y = 0
        ) => {
            /*
             * `graph` itself is positioned relative to its parent.
             */
            const graph_x =
                offset_x +
                (graph.x ?? 0);

            const graph_y =
                offset_y +
                (graph.y ?? 0);

            /*
             * Edges belong to this graph.
             *
             * They must use this graph's absolute position.
             */
            for (
                const edge of
                    graph.edges ?? []
            ) {
                edges.push(
                    flatten_edge(
                        edge,
                        graph_x,
                        graph_y
                    )
                );
            }

            /*
             * Children belong to this graph.
             */
            for (
                const child of
                    graph.children ?? []
            ) {
                const child_x =
                    graph_x +
                    (child.x ?? 0);

                const child_y =
                    graph_y +
                    (child.y ?? 0);

                nodes.push({
                    ...child,

                    x: child_x,
                    y: child_y,
                });

                walk(
                    child,
                    graph_x,
                    graph_y
                );
            }
        };

        walk(layout);

        return {
            nodes,
            edges,
        };
    };

    const edge_path = (
        edge
    ) =>
        (edge.sections ?? [])
            .map((section) => {
                const points = [
                    section.startPoint,
                    ...(section.bendPoints ?? []),
                    section.endPoint,
                ];

                return points
                    .map(
                        (
                            point,
                            index
                        ) =>
                            `${
                                index === 0
                                    ? "M"
                                    : "L"
                            }${point.x},${point.y}`
                    )
                    .join(" ");
            })
            .join(" ");

    let layout = $state(null);
    let error = $state(null);

    // ids of the barrier hosts that are currently opened
    let expanded = $state([]);

    const toggle_barrier = (id) => {
        expanded = expanded.includes(id)
            ? expanded.filter((other) => other !== id)
            : [...expanded, id];
    };

    // ids of the composite states whose regions are hidden
    let collapsed = $state([]);

    const toggle_collapse = (id) => {
        collapsed = collapsed.includes(id)
            ? collapsed.filter((other) => other !== id)
            : [...collapsed, id];
    };

    const toggleable = (data) =>
        data.expandable ||
        data.collapsed ||
        (data.regions?.length ?? 0) > 0;

    const toggle_node = (data) => {
        if (data.expandable) {
            toggle_barrier(data.id);
        } else if (toggleable(data)) {
            toggle_collapse(data.id);
        }
    };

    const barrier_map = $derived(
        new Map((model?.barriers ?? []).map((barrier) => [barrier.id, barrier]))
    );

    const roots = $derived(
        focus == null
            ? model?.roots
            : model?.barriers?.find((barrier) => barrier.id === focus)?.roots
    );

    // The root lists what its root args leave unmet; a barrier lists what its host must provide.
    const requirements = $derived(
        roots
            ? collect_issues(roots, barrier_map, focus == null ? root_props : [], null)
            : []
    );

    // Only the root tree is judged; a barrier body is judged where it is used.
    // The root is shown as one unnamed state so it lists what it provides and lacks like any state.
    const shown_roots = $derived(
        roots && focus == null
            ? [
                  {
                      id: "root",
                      display_name: "",
                      is_initial: false,
                      is_final: false,
                      is_barrier: false,
                      required_args: [],
                      provided_props: root_props,
                      actions: [],
                      transitions: [],
                      // direct_parent is reported by the states involved, not by the root
                      unsatisfied_args: requirements
                          .filter((issue) => !issue.dependency.is_direct_parent)
                          .map((issue) => ({
                          ...issue,
                          // the unnamed root has no own barrier to skip
                          barrier_path: issue.barrier_path.length > 0 ? ["", ...issue.barrier_path] : [],
                      })),
                      regions: [annotate_issues(roots, barrier_map, root_props, null)],
                  },
              ]
            : roots
    );

    $effect(() => {
        // the layout depends on the rows, which depend on this
        void show_props;

        if (!shown_roots) {
            return;
        }

        let cancelled = false;

        layout_graph(
            shown_roots,
            model.barriers ?? [],
            new Set(expanded),
            new Set(collapsed),
            focus == null ? [] : null
        )
            .then((result) => {
                if (cancelled) {
                    return;
                }

                layout = result;
                error = null;
            })
            .catch((exception) => {
                if (cancelled) {
                    return;
                }

                layout = null;
                error = String(
                    exception
                );
            });

        return () => {
            cancelled = true;
        };
    });

    const flat = $derived(
        layout
            ? flatten_layout(
                  layout
              )
            : {
                  nodes: [],
                  edges: [],
              }
    );

    const nodes = $derived(
        flat.nodes
    );

    const edges = $derived(
        flat.edges
    );

    // id of the edge under the pointer
    let hovered = $state(null);

    // barrier_id of the barrier state under the pointer; every host of it is highlighted
    let hovered_barrier = $state(null);

    // id of the state under the pointer
    let hovered_state = $state(null);

    const enter = (data) => {
        hovered_state = data.id;
        if (data.is_barrier) {
            hovered_barrier = data.barrier_id;
        }
    };

    const leave = () => {
        hovered_state = null;
        hovered_barrier = null;
    };

    const is_peer = (node) =>
        hovered_barrier != null &&
        node.data.is_barrier &&
        node.data.barrier_id === hovered_barrier;

    const hovered_edge = $derived(
        edges.find((edge) => edge.id === hovered)
    );

    // Containers are drawn under the edges so their fill does not hide the transitions inside.
    const is_container = (node) =>
        node.kind === "region" ||
        node.composite === true;

    const containers = $derived(
        nodes.filter(is_container)
    );

    const leaves = $derived(
        nodes.filter(
            (node) => !is_container(node)
        )
    );
</script>

<div class="model-container" class:dark={theme === "dark"}>
    {#if focus != null && requirements.length > 0}
        <div class="requirements">
            requires:
            {[...new Set(requirements.map((entry) => entry.dependency.type_name))].join(", ")}
        </div>
    {/if}
    {#if error}
        <pre class="error">{error}</pre>
    {:else if layout}
        <svg
            width={layout.width ?? 0}
            height={layout.height ?? 0}
            viewBox={`0 0 ${layout.width ?? 0} ${layout.height ?? 0}`}
        >
            <defs>
                <marker
                    id="arrow"
                    viewBox="0 0 10 10"
                    refX="9"
                    refY="5"
                    markerWidth="5"
                    markerHeight="5"
                    orient="auto-start-reverse"
                >
                    <path
                        d="M0,0 L10,5 L0,10 z"
                        class="arrowhead"
                    />
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
                    <path
                        d="M0,0 L10,5 L0,10 z"
                        class="arrowhead hot"
                    />
                </marker>
            </defs>

            {#each containers as node (node.id)}
                {@render draw(node)}
            {/each}

            <!--
                Draw edges over the containers.
            -->
            {#each edges as edge (edge.id)}
                <g
                    role="presentation"
                    class="edge"
                    onmouseenter={() => (hovered = edge.id)}
                    onmouseleave={() => (hovered = null)}
                >
                    <!-- wide invisible stroke so the thin line is easy to hover -->
                    <path class="hit" d={edge_path(edge)} />

                    <path
                        d={edge_path(edge)}
                        class="line"
                        class:capture={
                            edge.capture
                        }
                        marker-end="url(#arrow)"
                    />

                    {#each edge.labels ?? [] as label}
                        <text
                            x={
                                label.x +
                                label.width / 2
                            }
                            y={
                                label.y +
                                label.height / 2
                            }
                            class="center label"
                        >
                            {label.text}
                        </text>
                    {/each}
                </g>
            {/each}

            <!-- the hovered edge is drawn again on top, so reordering never disturbs the hover -->
            {#if hovered_edge}
                <g class="edge hot overlay">
                    <path
                        d={edge_path(hovered_edge)}
                        class="line"
                        marker-end="url(#arrow-hot)"
                    />

                    {#each hovered_edge.labels ?? [] as label}
                        <text
                            x={label.x + label.width / 2}
                            y={label.y + label.height / 2}
                            class="center label"
                        >
                            {label.text}
                        </text>
                    {/each}
                </g>
            {/if}

            <!-- the action block and the unprovided-args block, each with its divider, from y -->
            {#snippet action_rows(node, y)}
                {@const actions = action_texts(node.data)}
                {@const missing = missing_texts(node.data)}
                {@const provided = prop_texts(node.data)}

                {#if actions.length > 0}
                    <line
                        class="divider"
                        x1={node.x}
                        x2={node.x + node.width}
                        y1={y}
                        y2={y}
                    />

                    {#each actions as line, index}
                        <text
                            x={node.x + ACTION_PAD_X}
                            y={y + ACTION_PAD_Y + ACTION_LINE_H * (index + 0.5)}
                            class="action"
                        >
                            {line}
                        </text>
                    {/each}
                {/if}

                {#if missing.length > 0}
                    {@const top = y + block_height(actions.length)}

                    <line
                        class="divider"
                        x1={node.x}
                        x2={node.x + node.width}
                        y1={top}
                        y2={top}
                    />

                    {#each missing as line, index}
                        <text
                            x={node.x + ACTION_PAD_X}
                            y={top + ACTION_PAD_Y + ACTION_LINE_H * (index + 0.5)}
                            class="action missing"
                        >
                            {line}
                        </text>
                    {/each}
                {/if}

                {#if provided.length > 0}
                    {@const top = y + block_height(actions.length) + block_height(missing.length)}

                    <line
                        class="divider"
                        x1={node.x}
                        x2={node.x + node.width}
                        y1={top}
                        y2={top}
                    />

                    {#each provided as line, index}
                        <text
                            x={node.x + ACTION_PAD_X}
                            y={top + ACTION_PAD_Y + ACTION_LINE_H * (index + 0.5)}
                            class="action"
                        >
                            {line}
                        </text>
                    {/each}
                {/if}
            {/snippet}

            <!-- expand/collapse icon: a box with a minus (open) or a plus (closed) -->
            {#snippet toggle_icon(node, plus, y)}
                <g
                    class="toggle"
                    role="presentation"
                    onclick={() => toggle_node(node.data)}
                >
                    <rect x={node.x + node.width - 16} y={y} width="10" height="10" rx="2" />
                    <line x1={node.x + node.width - 14} x2={node.x + node.width - 8} y1={y + 5} y2={y + 5} />
                    {#if plus}
                        <line x1={node.x + node.width - 11} x2={node.x + node.width - 11} y1={y + 2} y2={y + 8} />
                    {/if}
                </g>
            {/snippet}

            {#snippet draw(node)}
                {#if node.kind === "start"}
                    <circle
                        class="start"
                        cx={
                            node.x +
                            node.width / 2
                        }
                        cy={
                            node.y +
                            START_SIZE / 2
                        }
                        r={
                            node.width / 2
                        }
                    />

                {:else if node.kind === "end"}
                    <circle
                        class="ring"
                        cx={
                            node.x +
                            node.width / 2
                        }
                        cy={
                            node.y +
                            node.height / 2
                        }
                        r={
                            node.width / 2
                        }
                    />

                    <circle
                        class="end"
                        cx={
                            node.x +
                            node.width / 2
                        }
                        cy={
                            node.y +
                            node.height / 2
                        }
                        r={
                            node.width / 2 - 3
                        }
                    />

                {:else if node.kind === "region"}
                    <!--
                        Region is a container, not a state.
                    -->
                    <rect
                        x={node.x}
                        y={node.y}
                        width={node.width}
                        height={node.height}
                        class="region"
                    />

                    <text
                        x={node.x + 8}
                        y={node.y + 14}
                        class="region-title"
                    >
                        Region {
                            node.region_index +
                                1
                        }
                    </text>

                {:else}
                    {@const composite =
                        node.composite ===
                        true}

                    {#if composite}
                        <!--
                            Composite state.
                        -->
                        <rect
                            x={node.x}
                            y={node.y}
                            width={node.width}
                            height={node.height}
                            rx="6"
                            class="composite"
                            class:barrier={
                                node.data
                                    .is_barrier
                            }
                            class:peer={is_peer(node)}
                            class:focus={hovered_state === node.data.id}
                            class:link={toggleable(node.data)}
                            onmouseenter={() => enter(node.data)}
                            onmouseleave={leave}
                            onclick={() => toggle_node(node.data)}
                        />

                        <text
                            x={
                                node.x +
                                10
                            }
                            y={
                                node.y +
                                20
                            }
                            class="composite-title"
                        >
                            {
                                node.data
                                    .display_name
                            }
                        </text>

                        {@render toggle_icon(node, false, node.y + 9)}

                        {#if actions_height(node.data) > 0}
                            {@render action_rows(
                                node,
                                node.y + COMPOSITE_TITLE_H
                            )}

                            <line
                                class="divider"
                                x1={node.x}
                                x2={node.x + node.width}
                                y1={node.y + COMPOSITE_TITLE_H + actions_height(node.data)}
                                y2={node.y + COMPOSITE_TITLE_H + actions_height(node.data)}
                            />
                        {/if}
                    {:else}
                        <rect
                            x={node.x}
                            y={node.y}
                            width={node.width}
                            height={node.height}
                            rx="5"
                            class:barrier={
                                node.data
                                    .is_barrier
                            }
                            class:collapsed={node.data.collapsed}
                            class:peer={is_peer(node)}
                            class:focus={hovered_state === node.data.id}
                            class:link={toggleable(node.data)}
                            onmouseenter={() => enter(node.data)}
                            onmouseleave={leave}
                            onclick={() => toggle_node(node.data)}
                        >
                            {#if node.data.collapsed}
                                <title>{node.data.region_count} region(s) hidden, click to show</title>
                            {/if}
                        </rect>

                        {#if node.data.collapsed}
                            <!-- a second, inner outline marks a state that hides regions -->
                            <rect
                                class="inset"
                                x={node.x + 3}
                                y={node.y + 3}
                                width={node.width - 6}
                                height={NODE_H - 6}
                                rx="3"
                            />
                        {/if}

                        {#if toggleable(node.data)}
                            {@render toggle_icon(node, true, node.y + 10)}
                        {/if}

                        <text
                            x={
                                node.x +
                                node.width /
                                    2
                            }
                            y={
                                node.y +
                                NODE_H / 2
                            }
                            class="center node-text"
                        >
                            {
                                node.data
                                    .display_name
                            }
                        </text>

                        {#if actions_height(node.data) > 0}
                            {@render action_rows(
                                node,
                                node.y + NODE_H
                            )}
                        {/if}
                    {/if}
                {/if}
            {/snippet}

            {#each leaves as node (node.id)}
                {@render draw(node)}
            {/each}
        </svg>
    {/if}
</div>

<style>
    .model-container {
        --canvas: #ffffff;
        --canvas-edge: #e2e8f0;
        --state: #ffffff;
        --composite: #f8fafc;
        --region: #ffffff;
        --stroke: #475569;
        --stroke-strong: #334155;
        --stroke-soft: #94a3b8;
        --text: #1e293b;
        --text-strong: #334155;
        --text-muted: #64748b;
        --edge: #475569;
        --edge-label: #0284c7;
        --edge-hot: #2563eb;
        --terminal: #0f172a;
        --barrier-fill: #eef2ff;
        --barrier-stroke: #6366f1;
        --peer-fill: #fce7f3;
        --peer-stroke: #db2777;
        --collapsed-fill: #ecfeff;
        --collapsed-stroke: #0891b2;
        --missing-text: #dc2626;
        --error-border: #fecaca;
        --error-bg: #fef2f2;
        --error-text: #ef4444;

        color-scheme: light;
        color: var(--text);

        display: flex;
        flex-direction: column;
        gap: 8px;
        align-items: flex-start;
    }

    .model-container.dark {
        --canvas: #0f0f0f;
        --canvas-edge: #262626;
        --state: #262626;
        --composite: #1a1a1a;
        --region: #121212;
        --stroke: #a3a3a3;
        --stroke-strong: #d4d4d4;
        --stroke-soft: #525252;
        --text: #e5e5e5;
        --text-strong: #d4d4d4;
        --text-muted: #a3a3a3;
        --edge: #a3a3a3;
        --edge-label: #5eead4;
        --edge-hot: #facc15;
        --terminal: #e5e5e5;
        --barrier-fill: #2a2433;
        --barrier-stroke: #a78bfa;
        --peer-fill: #3a1f2e;
        --peer-stroke: #f472b6;
        --collapsed-fill: #12302f;
        --collapsed-stroke: #5eead4;
        --missing-text: #f87171;
        --error-border: #7f1d1d;
        --error-bg: #3b0a0a;
        --error-text: #fca5a5;

        color-scheme: dark;
    }

    svg {
        font-family:
            system-ui,
            -apple-system,
            BlinkMacSystemFont,
            "Segoe UI",
            Roboto,
            sans-serif;

        font-size: 11px;

        background: var(--canvas);

        border-radius: 6px;

        box-shadow:
            inset 0 0 0 1px
            var(--canvas-edge);
    }

    /*
     * Ordinary state.
     */
    rect {
        fill: var(--state);

        stroke: var(--stroke);
        stroke-width: 1.25;
    }

    /*
     * Composite state.
     */
    rect.composite {
        fill: var(--composite);

        stroke: var(--stroke-strong);
        stroke-width: 1.5;
    }

    /*
     * Region container.
     *
     * Regions are deliberately visually distinct from states.
     */
    rect.region {
        fill: var(--region);

        stroke: var(--stroke-soft);
        stroke-width: 1;

        stroke-dasharray: 4 3;
    }

    /*
     * Barrier states.
     */
    rect.barrier {
        fill: var(--barrier-fill);
        stroke: var(--barrier-stroke);
        stroke-dasharray: 4 2;
    }

    rect.barrier.peer {
        fill: var(--peer-fill);
        stroke: var(--peer-stroke);
        stroke-width: 2;
    }

    /* a state that hides its regions */
    rect.collapsed {
        fill: var(--collapsed-fill);
        stroke: var(--collapsed-stroke);
    }

    rect.inset {
        fill: none;
        stroke: var(--collapsed-stroke);
        stroke-dasharray: 2 2;
        pointer-events: none;
    }

    /* the state under the pointer */
    rect.focus {
        stroke: var(--edge-hot);
        stroke-width: 2;
    }

    /* expand/collapse icon */
    g.toggle {
        cursor: pointer;
    }

    g.toggle rect {
        fill: var(--canvas);
        stroke: var(--stroke);
        stroke-width: 1;
    }

    g.toggle line {
        stroke: var(--stroke);
        stroke-width: 1.25;
    }

    rect.link {
        cursor: pointer;
    }

    rect.link:hover {
        filter: brightness(0.96);
    }

    /* clicks must reach the rect underneath */
    text.composite-title,
    text.node-text {
        pointer-events: none;
    }

    /*
     * Initial pseudo-node.
     */
    circle.start {
        fill: var(--terminal);
        stroke: none;
    }

    /*
     * Final pseudo-node.
     */
    circle.ring {
        fill: var(--canvas);

        stroke: var(--terminal);
        stroke-width: 1.5;
    }

    circle.end {
        fill: var(--terminal);
        stroke: none;
    }

    /*
     * Transitions.
     */
    path {
        fill: none;

        stroke: var(--edge);
        stroke-width: 1.25;
    }

    path.arrowhead {
        fill: var(--edge);
        stroke: none;
    }

    path.arrowhead.hot {
        fill: var(--edge-hot);
    }

    /* invisible, wide, so the thin line is easy to hover */
    path.hit {
        stroke: transparent;
        stroke-width: 12;
        pointer-events: stroke;
    }

    path.line {
        pointer-events: none;
    }

    g.hot path.line {
        stroke: var(--edge-hot);
        stroke-width: 2.25;
    }

    g.hot text.label {
        fill: var(--edge-hot);
        font-weight: 700;

        /* hides the plain label underneath */
        stroke: var(--canvas);
        stroke-width: 3px;
        paint-order: stroke;
    }

    g.overlay {
        pointer-events: none;
    }

    path.capture {
        stroke-dasharray: 3 2;
    }

    /*
     * Text.
     */
    text {
        fill: var(--text);
    }

    text.node-text {
        font-weight: 500;
    }

    text.composite-title {
        fill: var(--text-strong);

        font-size: 10px;
        font-weight: 600;

        letter-spacing: 0.02em;
    }

    line.divider {
        stroke: var(--stroke-soft);
        stroke-width: 1;
        pointer-events: none;
    }

    text.action {
        fill: var(--text-muted);

        font-size: 10px;
        dominant-baseline: central;
        pointer-events: none;
    }

    text.action.missing {
        fill: var(--missing-text);
    }

    .requirements {
        font-size: 11px;
        color: var(--text-muted);
    }

    text.region-title {
        fill: var(--text-muted);

        font-size: 9px;
        font-weight: 500;

        letter-spacing: 0.03em;
    }

    text.center {
        text-anchor: middle;
        dominant-baseline: central;
    }

    text.label {
        fill: var(--edge-label);

        font-size: 10px;
        font-weight: 500;
    }

    .error {
        padding: 10px;

        border: 1px solid var(--error-border);
        border-radius: 4px;

        background: var(--error-bg);
        color: var(--error-text);

        font-size: 12px;
    }
</style>
