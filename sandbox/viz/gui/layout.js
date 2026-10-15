/*
 * Lays the states out. `layout_graph` places one level of states with ELK, a composite's regions
 * side by side inside its box, and re-routes the transitions with routing.js; `flatten_layout`
 * turns the nested result into lists of nodes and edges in diagram coordinates.
 */
import {
    COMPOSITE,
    LAYERED_OPTIONS,
    NODE,
    REGION,
    REGION_PADDING,
    ROUTE,
    TERMINATION,
} from "./constants.js";
import {
    actions_height,
    actions_width,
    edge_text_width,
    has_title,
    state_width,
    text_width,
} from "./measure.js";
import { route_edges } from "./routing.js";

const { H: NODE_H, START_SIZE, START_GAP, END_SIZE, TOGGLE_W } = NODE;
const { GAP: REGION_GAP } = REGION;
const {
    PAD_TOP: COMPOSITE_PAD_TOP,
    PAD_TOP_UNTITLED: COMPOSITE_PAD_TOP_UNTITLED,
    PAD_X: COMPOSITE_PAD_X,
    PAD_BOTTOM: COMPOSITE_PAD_BOTTOM,
} = COMPOSITE;
const { ROOT_PADDING, EDGE_MARGIN } = ROUTE;

// Edge ids keep counting across layouts: a superseded layout may still be running.
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
const build_simple_node = (node, show_props) => ({
    id: node.id,
    kind: "state",
    data: node,
    width: Math.max(
        state_width(node) +
            (node.collapsed || node.expandable ? TOGGLE_W : 0),
        actions_width(node, show_props)
    ),
    height: NODE_H + actions_height(node, show_props),
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
 *
 * `env` is { elk, show_props }: the ELK instance and whether props take a row.
 */
const layout_region = async (
    nodes,
    scope,
    env,
    padding = ROOT_PADDING
) => {
    const real = visible_nodes(nodes);

    const children = [];
    // Composites go to ELK as opaque boxes; their laid-out content is re-attached afterwards.
    const composites = new Map();

    for (const node of real) {
        if ((node.regions?.length ?? 0) === 0) {
            children.push(
                build_simple_node(node, env.show_props)
            );
        } else {
            const composite =
                await layout_composite_state(
                    node,
                    scope,
                    env
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

    const result = await env.elk.layout({
        id: `${scope}/graph`,

        layoutOptions: {
            ...LAYERED_OPTIONS,
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
 * Layout a composite state.
 *
 * Every region is rendered simultaneously.
 *
 * Regions are siblings and are placed horizontally.
 */
const layout_composite_state = async (
    node,
    parent_scope,
    env
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
                env,
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
    const titled = has_title(node);
    const top =
        (titled ? COMPOSITE_PAD_TOP : COMPOSITE_PAD_TOP_UNTITLED) +
        actions_height(node, env.show_props);

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
        titled ? text_width(node.display_name) + 10 + TOGGLE_W : 0,
        actions_width(node, env.show_props)
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

export const layout_graph = (roots, env) => layout_region(roots, "root", env);

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
export const flatten_layout = (
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
