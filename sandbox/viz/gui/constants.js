/*
 * The numbers the viewer is built from: sizes and spacing in px, timing, and the ELK options.
 * They are grouped by what they measure; the groups the drawing components need are handed to
 * them through the "viz" context in Model.svelte.
 */
export const TERMINATION = "[**]";

// Sizes in px, grouped by what they measure.
export const NODE = {
    H: 30,
    START_SIZE: 12,
    // room below the start circle gives its transition the length of a labelled one
    START_GAP: 26,
    END_SIZE: 26,
    // room at the right of a title for the expand/collapse icon
    TOGGLE_W: 18,
};

export const TEXT = { CHAR_W: 6.5, PAD: 18 };

// the rows under a state's name: hooks and event/capture responses
export const ROWS = { CHAR_W: 5.8, LINE_H: 13, PAD_X: 8, PAD_Y: 5 };

export const REGION = { PAD_X: 10, PAD_TOP: 24, PAD_BOTTOM: 10, GAP: 10 };

// ELK padding of the graph inside a region
export const REGION_PADDING = `[top=${REGION.PAD_TOP},left=${REGION.PAD_X},bottom=${REGION.PAD_BOTTOM},right=${REGION.PAD_X}]`;

export const COMPOSITE = {
    PAD_TOP: 34,
    // above the regions of a state that has no name row (the root)
    PAD_TOP_UNTITLED: 10,
    PAD_X: 10,
    PAD_BOTTOM: 10,
    TITLE_H: NODE.H,
};

export const ROUTE = {
    // room for the loops that run around the outside of the top-level states
    ROOT_PADDING: "[top=20,left=20,bottom=20,right=20]",
    LABEL_GAP: 4,
    // shortest leg of an L route, so the arrowhead is never the whole segment
    LEG: 18,
    // preferred distance between a route and the nodes it is not attached to
    CLEARANCE: 12,
    // preferred distance between parallel ports
    PORT_GAP: 10,
    // closest a route or label may come to the edge of the region that holds it
    EDGE_MARGIN: 6,
};

// the move between a collapsed and an expanded state; the fade is a window of its progress
export const ZOOM = { MS: 450, FADE_START: 0.25, FADE_SPAN: 0.5 };

// levels fetched from the server that are kept, oldest dropped first
export const CACHE_LIMIT = 256;

// Standard state graph.
export const LAYERED_OPTIONS = {
    "elk.algorithm": "layered",
    "elk.direction": "DOWN",

    "elk.edgeRouting": "ORTHOGONAL",

    "elk.layered.layering.strategy": "NETWORK_SIMPLEX",
    "elk.layered.cycleBreaking.strategy": "DEPTH_FIRST",

    "elk.layered.nodePlacement.strategy": "BRANDES_KOEPF",
    "elk.layered.nodePlacement.favorStraightEdges": "true",
    "elk.layered.nodePlacement.bk.edgeStraightening": "IMPROVE_STRAIGHTNESS",

    "elk.layered.spacing.nodeNodeBetweenLayers": "18",
    "elk.spacing.nodeNode": "14",
    "elk.layered.spacing.edgeNodeBetweenLayers": "10",
    "elk.layered.spacing.edgeEdgeBetweenLayers": "6",

    "elk.layered.crossingMinimization.strategy": "LAYER_SWEEP",
    "elk.layered.compaction.postCompaction": true,
    "elk.layered.mergeEdges": false,

    "elk.separateConnectedComponents": false,
};
