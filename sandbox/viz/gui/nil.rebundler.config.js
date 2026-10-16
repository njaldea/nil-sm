export default {
    in: ".",
    out: "../assets",
    mode: {
        type: "es",
        file: "index.js",
        plugins: ["svelte", "terser", "resolve"]
    }
};