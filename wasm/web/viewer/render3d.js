// Drawing of the bins of box and boxstacks ('solution.js', 'dimensions' 3):
// an isometric view of a bin, as an SVG image, seen from above one of its
// corners. The items are drawn from the back to the front (painter's
// algorithm), so that the items in front hide the ones behind them.

import {itemColor} from "./render.js";

const SVG = "http://www.w3.org/2000/svg";

const COS = Math.cos(Math.PI / 6);
const SIN = Math.sin(Math.PI / 6);

function svgElement(name, attributes = {}) {
    const element = document.createElementNS(SVG, name);
    for (const [key, value] of Object.entries(attributes))
        element.setAttribute(key, value);
    return element;
}

// The point [x, y, z] on the image: the x axis to the right and to the back,
// the y axis to the left and to the back, the z axis up (the y axis of the
// image points down). The corner of the bin at the origin is the nearest
// one.
export function project([x, y, z]) {
    return [(x - y) * COS, -((x + y) * SIN + z)];
}

// The box 'box' of a bin of size 'size' ({x, y}), seen from the corner
// 'rotation' (0 to 3, a quarter turn around the vertical axis each): in the
// frame of the view, whose origin is the nearest corner of the bin.
export function rotateBox(box, size, rotation) {
    const corners = [[box.x0, box.y0], [box.x1, box.y1]].map(([x, y]) => {
        switch (((rotation % 4) + 4) % 4) {
        case 1: return [y, size.x - x];
        case 2: return [size.x - x, size.y - y];
        case 3: return [size.y - y, x];
        default: return [x, y];
        }
    });
    return {
        x0: Math.min(corners[0][0], corners[1][0]), x1: Math.max(corners[0][0], corners[1][0]),
        y0: Math.min(corners[0][1], corners[1][1]), y1: Math.max(corners[0][1], corners[1][1]),
        z0: box.z0, z1: box.z1,
    };
}

// The size of the frame of the bins of a solution: their largest dimensions,
// so that all the bins are drawn at the same scale.
export function binsFrame3(bins) {
    const size = (key0, key1) => Math.max(0, ...bins.map((bin) => bin.box3[key1] - bin.box3[key0]));
    return {x: size("x0", "x1"), y: size("y0", "y1"), z: size("z0", "z1")};
}

function screenBox(box) {
    const points = [];
    for (const x of [box.x0, box.x1])
        for (const y of [box.y0, box.y1])
            for (const z of [box.z0, box.z1])
                points.push(project([x, y, z]));
    return {
        u0: Math.min(...points.map((p) => p[0])), u1: Math.max(...points.map((p) => p[0])),
        v0: Math.min(...points.map((p) => p[1])), v1: Math.max(...points.map((p) => p[1])),
    };
}

// 'a' is behind 'b' (two boxes which don't overlap): farther along x or y, or
// below.
function behind(a, b) {
    return a.x0 >= b.x1 || a.y0 >= b.y1 || a.z1 <= b.z0;
}

// The order in which to draw boxes (in the frame of the view) so that each
// box is drawn after the boxes behind it: a topological order of the relation
// 'behind', between the boxes whose images overlap. In the rare case of a
// cycle, the remaining boxes are drawn from the farthest to the nearest.
export function paintOrder(boxes) {
    const n = boxes.length;
    const screens = boxes.map(screenBox);
    const successors = boxes.map(() => []);
    const predecessors = new Array(n).fill(0);
    const eps = 1e-9;
    for (let i = 0; i < n; ++i) {
        for (let j = i + 1; j < n; ++j) {
            const a = screens[i];
            const b = screens[j];
            if (a.u1 <= b.u0 + eps || b.u1 <= a.u0 + eps || a.v1 <= b.v0 + eps || b.v1 <= a.v0 + eps)
                continue;
            if (behind(boxes[i], boxes[j])) {
                successors[i].push(j);
                predecessors[j]++;
            } else if (behind(boxes[j], boxes[i])) {
                successors[j].push(i);
                predecessors[i]++;
            }
        }
    }
    const order = [];
    const ready = [];
    for (let i = 0; i < n; ++i)
        if (predecessors[i] === 0)
            ready.push(i);
    const done = new Array(n).fill(false);
    while (order.length < n) {
        if (ready.length === 0) {
            // A cycle: the farthest box left.
            const depth = (i) => boxes[i].x0 + boxes[i].y0 - boxes[i].z0;
            let farthest = -1;
            for (let i = 0; i < n; ++i)
                if (!done[i] && (farthest === -1 || depth(i) > depth(farthest)))
                    farthest = i;
            ready.push(farthest);
        }
        const i = ready.pop();
        if (done[i])
            continue;
        done[i] = true;
        order.push(i);
        for (const j of successors[i])
            if (--predecessors[j] === 0 && !done[j])
                ready.push(j);
    }
    return order;
}

// A color 'rgb(r, g, b)' darkened by 'factor' (1: unchanged).
function shade(color, factor) {
    const match = /rgb\((\d+),\s*(\d+),\s*(\d+)\)/.exec(color);
    if (match === null)
        return color;
    const [r, g, b] = match.slice(1).map((v) => Math.round(Number(v) * factor));
    return `rgb(${r}, ${g}, ${b})`;
}

function polygon(points, attributes) {
    return svgElement("polygon", {
        points: points.map((p) => project(p).join(",")).join(" "),
        ...attributes,
    });
}

// The three visible faces of a box: its top, and its faces towards the
// viewer (x = x0 and y = y0).
function boxFaces(box) {
    const {x0, y0, z0, x1, y1, z1} = box;
    return {
        top: [[x0, y0, z1], [x1, y0, z1], [x1, y1, z1], [x0, y1, z1]],
        left: [[x0, y0, z0], [x0, y1, z0], [x0, y1, z1], [x0, y0, z1]],
        right: [[x0, y0, z0], [x1, y0, z0], [x1, y0, z1], [x0, y0, z1]],
    };
}

// The SVG image of a bin of box or boxstacks. 'detail': with the ids of the
// item types on the items (if they fit) and a tooltip on each item. 'frame':
// the size of the image ('binsFrame3'), the bin being drawn with its nearest
// corner at the same place in all the images; the size of the bin by default.
// 'rotation': the corner from which the bin is seen (0 to 3).
export function renderBin3(bin, {detail = false, frame = null, rotation = 0} = {}) {
    const binSize = {x: bin.box3.x1 - bin.box3.x0, y: bin.box3.y1 - bin.box3.y0, z: bin.box3.z1 - bin.box3.z0};
    const quarter = (((rotation % 4) + 4) % 4) % 2 === 1;
    const frameSize = (frame !== null)? frame: binSize;
    // The frame, in the frame of the view.
    const view = quarter?
        {x0: 0, y0: 0, z0: 0, x1: frameSize.y, y1: frameSize.x, z1: frameSize.z}:
        {x0: 0, y0: 0, z0: 0, x1: frameSize.x, y1: frameSize.y, z1: frameSize.z};
    const bounds = screenBox(view);
    const margin = 0.03 * Math.max(bounds.u1 - bounds.u0, bounds.v1 - bounds.v0);
    const svg = svgElement("svg", {
        viewBox: `${bounds.u0 - margin} ${bounds.v0 - margin}`
            + ` ${bounds.u1 - bounds.u0 + 2 * margin} ${bounds.v1 - bounds.v0 + 2 * margin}`,
        class: "viewer-bin viewer-bin-3d" + (detail? " detail": ""),
        role: "img",
    });
    const local = (box) => rotateBox({
        x0: box.x0 - bin.box3.x0, x1: box.x1 - bin.box3.x0,
        y0: box.y0 - bin.box3.y0, y1: box.y1 - bin.box3.y0,
        z0: box.z0 - bin.box3.z0, z1: box.z1 - bin.box3.z0,
    }, binSize, rotation);
    const binBox = local(bin.box3);
    const {x0, y0, z0, x1, y1, z1} = binBox;

    // The back of the bin: its floor and its two walls at the back.
    svg.appendChild(polygon([[x0, y0, z0], [x1, y0, z0], [x1, y1, z0], [x0, y1, z0]], {class: "viewer-bin-floor"}));
    svg.appendChild(polygon([[x1, y0, z0], [x1, y1, z0], [x1, y1, z1], [x1, y0, z1]], {class: "viewer-bin-wall"}));
    svg.appendChild(polygon([[x0, y1, z0], [x1, y1, z0], [x1, y1, z1], [x0, y1, z1]], {class: "viewer-bin-wall"}));

    // The defects, then the items, from the back to the front.
    const boxes = [
        ...bin.defects3.map((defect) => ({box: local(defect), defect: true})),
        ...bin.items.map((item) => ({box: local(item.box3), item})),
    ];
    // The ids of the item types on the tops of the items, if they fit, drawn
    // with their items: the items in front of them cover them.
    const bounds2 = Math.max(bounds.u1 - bounds.u0, bounds.v1 - bounds.v0);
    const maximumFontSize = 0.035 * bounds2;
    const minimumFontSize = 0.012 * bounds2;
    const itemLabel = (box, item) => {
        // Not on the items with an item above them.
        const covered = boxes.some(({box: other, item: otherItem}) => otherItem !== undefined
            && other !== box && other.z0 >= box.z1
            && other.x0 < box.x1 && box.x0 < other.x1 && other.y0 < box.y1 && box.y0 < other.y1);
        if (covered)
            return null;
        const text = String(item.itemTypeId);
        const topWidth = Math.min(box.x1 - box.x0, box.y1 - box.y0) * COS;
        const fontSize = Math.min(maximumFontSize, topWidth / (0.9 * text.length));
        if (fontSize < minimumFontSize)
            return null;
        const [u, v] = project([(box.x0 + box.x1) / 2, (box.y0 + box.y1) / 2, box.z1]);
        const label = svgElement("text", {x: u, y: v, "font-size": fontSize, class: "viewer-item-label"});
        label.textContent = text;
        return label;
    };
    for (const index of paintOrder(boxes.map((b) => b.box))) {
        const {box, item} = boxes[index];
        const faces = boxFaces(box);
        const group = svgElement("g", {class: (item !== undefined)? "viewer-item": "viewer-defect"});
        if (item === undefined) {
            group.appendChild(polygon(faces.top, {class: "viewer-defect-face"}));
            if (box.z1 > box.z0) {
                group.appendChild(polygon(faces.left, {class: "viewer-defect-face"}));
                group.appendChild(polygon(faces.right, {class: "viewer-defect-face"}));
            }
            if (detail) {
                const title = svgElement("title");
                title.textContent = "Defect";
                group.appendChild(title);
            }
        } else {
            const color = itemColor(item.itemTypeId);
            group.appendChild(polygon(faces.left, {fill: shade(color, 0.8)}));
            group.appendChild(polygon(faces.right, {fill: shade(color, 0.65)}));
            group.appendChild(polygon(faces.top, {fill: color}));
            if (detail) {
                const title = svgElement("title");
                title.textContent = item.label;
                group.appendChild(title);
                const label = itemLabel(box, item);
                if (label !== null)
                    group.appendChild(label);
            }
        }
        svg.appendChild(group);
    }

    // The front edges of the bin, over the items.
    const edges = [
        [[x0, y0, z0], [x0, y0, z1]],
        [[x0, y0, z1], [x1, y0, z1]], [[x0, y0, z1], [x0, y1, z1]],
        [[x1, y0, z1], [x1, y1, z1]], [[x0, y1, z1], [x1, y1, z1]],
        [[x0, y0, z0], [x1, y0, z0]], [[x0, y0, z0], [x0, y1, z0]],
    ];
    for (const [a, b] of edges) {
        const [pa, pb] = [project(a), project(b)];
        svg.appendChild(svgElement("line", {x1: pa[0], y1: pa[1], x2: pb[0], y2: pb[1], class: "viewer-bin-edge"}));
    }

    return svg;
}
