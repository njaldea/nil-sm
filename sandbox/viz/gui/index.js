import { mount, unmount } from 'svelte';
import Frame from "./Frame.svelte";

export const create_sm_component = async (target) => {
    const ref = mount(Frame, { target });
    return { destroy: () => unmount(ref) };
};
