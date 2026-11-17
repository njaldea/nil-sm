/*
 * Sizes of what a state shows: its name, and the rows under it (actions, unmet args and,
 * optionally, props). The layout sizes the boxes with these and the components draw the rows
 * with the same numbers.
 */
import { ROWS, TEXT } from "./constants.js";

export const text_width = (text = "") => Math.max(50, text.length * TEXT.CHAR_W + TEXT.PAD);

export const state_width = (node) => Math.max(70, text_width(node.display_name));

export const edge_text_width = (text = "") => Math.max(20, text.length * TEXT.CHAR_W + 8);

// the root is the one state without a name, so it has no name row
export const has_title = (data) => data.id !== "root";

// an emit names the event it emits
const response_text = (action) =>
    action.emitted_event ? `${action.response} ${action.emitted_event}` : action.response;

const action_text = (action) => {
    switch (action.type) {
        case "entry":
            return `on_enter / ${response_text(action)}`;
        case "exit":
            return `on_exit / ${response_text(action)}`;
        case "regions_finalized":
            return `on_regions_finalized / ${response_text(action)}`;
        case "capture":
            return `capture ${action.event} / ${response_text(action)}`;
        default:
            return `${action.event} / ${response_text(action)}`;
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
    return `missing ${name}${nested.length > 0 ? ` (${nested.join(" > ")})` : ""}`;
};

// Blocks under the name: the actions, the args nothing provides, then (optionally) the props it provides.
// A collapsed state also says how many unmet args it hides.
export const action_texts = (node) => (node?.actions ?? []).map(action_text);

export const missing_texts = (node) => [
    ...(node?.unsatisfied_args ?? []).map(missing_text),
    ...(node?.collapsed && node.subtree_unmet > 0 ? [`${node.subtree_unmet} unmet below`] : []),
];

export const prop_texts = (node, show_props) =>
    show_props ? (node?.provided_props ?? []).map((prop) => `provides ${prop.type_name}`) : [];

export const block_height = (count) =>
    count === 0 ? 0 : count * ROWS.LINE_H + 2 * ROWS.PAD_Y;

export const actions_height = (node, show_props) =>
    block_height(action_texts(node).length) +
    block_height(missing_texts(node).length) +
    block_height(prop_texts(node, show_props).length);

export const actions_width = (node, show_props) =>
    Math.max(
        0,
        ...[...action_texts(node), ...missing_texts(node), ...prop_texts(node, show_props)].map(
            (line) => line.length * ROWS.CHAR_W + 2 * ROWS.PAD_X
        )
    );
