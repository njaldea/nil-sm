/*
 * The easing of the slide between panes.
 */

// slow at both ends of the move
export const ease = (p) => (p < 0.5 ? 4 * p ** 3 : 1 - (-2 * p + 2) ** 3 / 2);
