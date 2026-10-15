/*
 * Loads the states from the server. `build_tree` fetches the levels that are open (and what each
 * state lacks, for the root tree), and `fetch_json` is the cached request it uses.
 */
import { CACHE_LIMIT, TERMINATION } from "./constants.js";

/*
 * The server hands out one level at a time (see viewer.hpp). Levels that were asked for
 * are kept, oldest dropped first, so reopening a state costs no request.
 */
const fetched = new Map();

export const fetch_json = (url) => {
    const hit = fetched.get(url);
    if (hit) {
        fetched.delete(url);
        fetched.set(url, hit);
        return hit;
    }

    const request = fetch(url)
        .then((response) => response.json())
        .then((body) => {
            if (body.error) {
                throw new Error(`${url}: ${body.error}`);
            }
            return body;
        })
        .catch((failure) => {
            fetched.delete(url);
            throw failure;
        });

    fetched.set(url, request);
    if (fetched.size > CACHE_LIMIT) {
        fetched.delete(fetched.keys().next().value);
    }
    return request;
};

/*
 * Ids of a barrier body are shared by every host of it, so each occurrence gets its host's
 * id as a prefix to stay unique across the diagram. The server judges by the same ids.
 */
const place_node = (prefix, node, report) => ({
    ...node,
    id: `${prefix}${node.id}`,
    transitions: (node.transitions ?? []).map((transition) => ({
        ...transition,
        target:
            transition.target === TERMINATION
                ? transition.target
                : `${prefix}${transition.target}`,
    })),
    unsatisfied_args: report?.unsatisfied_args ?? [],
    subtree_unmet: report?.subtree_unmet ?? 0,
});

/*
 * The regions of one state. `state_id` is its id within `scope` (null: the top of the scope),
 * `parent_id` its id in the diagram and `val_key` the id the server judges it by.
 * Only states in `open_ids` bring their own regions; the others stay collapsed.
 * `placement` records where each state sits so that opening one can close its siblings.
 * `judged` is false for a barrier shown on its own: it is judged where it is hosted, not here.
 */
const load_regions = async (
    judged,
    scope,
    state_id,
    parent_id,
    val_key,
    prefix,
    open_ids,
    placement
) => {
    const [level, validation] = await Promise.all([
        fetch_json(state_id == null ? `/model/${scope}` : `/model/${scope}/${state_id}`),
        judged ? fetch_json(`/model_val/${val_key}`) : null,
    ]);
    const reports = new Map(
        (validation?.children ?? []).map((report) => [report.id, report])
    );

    return Promise.all(
        level.regions.map((region, index) =>
            Promise.all(
                region.map(async (state) => {
                    const node = place_node(prefix, state, reports.get(state.id));
                    placement.set(node.id, {
                        parent: parent_id,
                        region: `${parent_id}/${index}`,
                    });

                    if (state.barrier_id != null) {
                        // a host opens onto a copy of its barrier body
                        node.expandable = true;
                        if (open_ids.has(node.id)) {
                            node.regions = await load_regions(
                                judged,
                                state.barrier_id,
                                null,
                                node.id,
                                node.id,
                                `${node.id}::`,
                                open_ids,
                                placement
                            );
                        }
                    } else if ((state.region_sizes ?? []).length > 0) {
                        if (open_ids.has(node.id)) {
                            node.regions = await load_regions(
                                judged,
                                scope,
                                state.id,
                                node.id,
                                node.id,
                                prefix,
                                open_ids,
                                placement
                            );
                        } else {
                            node.collapsed = true;
                            node.region_count = state.region_sizes.length;
                        }
                    }
                    return node;
                })
            )
        )
    );
};

const find_node = (nodes, id) => {
    for (const node of nodes) {
        if (node.id === id) {
            return node;
        }
        for (const region of node.regions ?? []) {
            const found = find_node(region, id);
            if (found) {
                return found;
            }
        }
    }
    return null;
};

/*
 * The states to lay out. `focus` is the barrier to show (null: the root), `focused` the open
 * state the diagram is narrowed to (null: none).
 * The root is shown as one unnamed state so it lists what it provides and lacks like any state.
 */
export const build_tree = async ({ focus, root_props, open_ids, focused }) => {
    const judged = focus == null;
    const placement = new Map();
    const regions = await load_regions(
        judged,
        focus ?? "0",
        null,
        "root",
        "0",
        "",
        open_ids,
        placement
    );

    // Its siblings are hidden, so its start marker and transitions would point at nothing.
    const focus_roots = (nodes) => {
        const node = focused == null ? null : find_node(nodes, focused);
        return node ? [{ ...node, is_initial: false, transitions: [] }] : nodes;
    };

    if (!judged) {
        return { roots: focus_roots(regions[0]), placement };
    }

    const { root } = await fetch_json("/model_val/0");
    const root_node = {
        id: "root",
        display_name: "",
        is_initial: false,
        is_final: false,
        is_barrier: false,
        required_args: [],
        provided_props: root_props,
        actions: [],
        transitions: [],
        unsatisfied_args: root,
        regions,
    };
    return { roots: focus_roots([root_node]), placement };
};
