// Drawing of the bins of the visualizer ('solution.js'): a two-dimensional bin
// as an SVG image, the bins of onedimensional as rows.

import {PASTEL_COLORS} from "../visualize/common.js";

const SVG = "http://www.w3.org/2000/svg";

// The color of the items of an item type (as the plotly visualizers).
export function itemColor(itemTypeId) {
    return PASTEL_COLORS[itemTypeId % PASTEL_COLORS.length];
}

function svgElement(name, attributes = {}) {
    const element = document.createElementNS(SVG, name);
    for (const [key, value] of Object.entries(attributes))
        element.setAttribute(key, value);
    return element;
}

// The 'd' attribute of a path ('solution.js'), the y axis pointing up: y is
// drawn at 'top - y'.
function pathData(path, top) {
    return path.map((contour) => "M" + contour.map(([x, y]) => `${x},${top - y}`).join("L") + "Z").join("");
}

function boxPath(box) {
    return [[[box.x0, box.y0], [box.x1, box.y0], [box.x1, box.y1], [box.x0, box.y1]]];
}

// The size of the frame of the bins of a solution: the largest width and the
// largest height of its bins, so that all its bins are drawn at the same
// scale.
export function binsFrame(bins) {
    return {
        width: Math.max(0, ...bins.map((bin) => bin.box.x1 - bin.box.x0)),
        height: Math.max(0, ...bins.map((bin) => bin.box.y1 - bin.box.y0)),
    };
}

// The SVG image of a two-dimensional bin. 'detail': with the ids of the item
// types on the items (if they fit) and a tooltip on each item; otherwise (the
// overview), the shapes only. 'frame': the size of the image ('binsFrame'),
// the bin being drawn in its bottom-left corner; the size of the bin by
// default.
export function renderBin(bin, {detail = false, frame = null} = {}) {
    const {x0, y0, x1, y1} = bin.box;
    const width = (frame !== null)? Math.max(frame.width, x1 - x0): x1 - x0;
    const height = (frame !== null)? Math.max(frame.height, y1 - y0): y1 - y0;
    const size = Math.max(width, height);
    const margin = 0.02 * size;
    // The y axis pointing up, the bottom of the bin at the bottom of the
    // frame.
    const top = y0 + height;
    const svg = svgElement("svg", {
        viewBox: `${x0 - margin} ${-margin} ${width + 2 * margin} ${height + 2 * margin}`,
        class: "viewer-bin" + (detail? " detail": ""),
        role: "img",
    });
    const path = (p, className, title = null) => {
        const element = svgElement("path", {d: pathData(p, top), class: className, "fill-rule": "evenodd"});
        if (title !== null) {
            const titleElement = svgElement("title");
            titleElement.textContent = title;
            element.appendChild(titleElement);
        }
        svg.appendChild(element);
        return element;
    };
    path(bin.outline, "viewer-bin-outline");
    for (const box of bin.trims)
        path(boxPath(box), "viewer-trim", detail? "Trim": null);
    for (const box of bin.wastes)
        path(boxPath(box), "viewer-waste", detail? "Waste": null);
    for (const box of bin.residuals)
        path(boxPath(box), "viewer-residual", detail? "Residual": null);
    // The cuts of rectangleguillotine: the edges of the nodes of the cutting
    // tree, thinner for the deeper cuts; below the defects and the items,
    // whose edges are drawn over them.
    for (const cut of bin.cuts) {
        const element = path(boxPath(cut.box), "viewer-cut");
        element.setAttribute("stroke-width", String(Math.max(0.5, 2 - 0.5 * cut.depth)));
    }
    for (const defect of bin.defects)
        path(defect, "viewer-defect", detail? "Defect": null);
    for (const item of bin.items) {
        for (const itemPath of item.paths) {
            const element = path(itemPath, "viewer-item", detail? item.label: null);
            element.setAttribute("fill", itemColor(item.itemTypeId));
        }
    }
    if (detail) {
        // The ids of the item types, if they fit in the items.
        const maximumFontSize = 0.04 * size;
        const minimumFontSize = 0.012 * size;
        for (const item of bin.items) {
            const box = item.labelBox || item.box;
            const itemWidth = box.x1 - box.x0;
            const itemHeight = box.y1 - box.y0;
            const text = String(item.itemTypeId);
            const fontSize = Math.min(maximumFontSize, 0.5 * itemHeight, itemWidth / (0.7 * text.length));
            if (fontSize < minimumFontSize)
                continue;
            const label = svgElement("text", {
                x: (box.x0 + box.x1) / 2,
                y: top - (box.y0 + box.y1) / 2,
                "font-size": fontSize,
                class: "viewer-item-label",
            });
            label.textContent = text;
            svg.appendChild(label);
        }
    }
    return svg;
}

// The bins of onedimensional: a row each, at the same scale, with the ids of
// the item types on the items (if they fit) and a tooltip on each item.
export function renderRows(solution) {
    const container = document.createElement("div");
    container.className = "viewer-rows";
    const length = Math.max(...solution.bins.map((bin) => bin.box.x1 - bin.box.x0));
    for (const bin of solution.bins) {
        const row = document.createElement("div");
        row.className = "viewer-row";
        const caption = document.createElement("div");
        caption.className = "viewer-row-caption";
        caption.textContent = `Bin ${bin.index}`;
        if (bin.copies > 1) {
            const copies = document.createElement("span");
            copies.className = "viewer-copies";
            copies.textContent = `×${bin.copies}`;
            caption.append(" ", copies);
        }
        const binLength = bin.box.x1 - bin.box.x0;
        const bar = document.createElement("div");
        bar.className = "viewer-row-bin";
        bar.style.width = `${100 * binLength / length}%`;
        bar.title = `Bin ${bin.index}: length ${binLength}`;
        for (const item of bin.items) {
            const segment = document.createElement("div");
            segment.className = "viewer-row-item";
            segment.style.left = `${100 * (item.box.x0 - bin.box.x0) / binLength}%`;
            segment.style.width = `${100 * (item.box.x1 - item.box.x0) / binLength}%`;
            segment.style.background = itemColor(item.itemTypeId);
            segment.title = item.label;
            segment.textContent = String(item.itemTypeId);
            bar.appendChild(segment);
        }
        row.append(caption, bar);
        container.appendChild(row);
    }
    return container;
}
