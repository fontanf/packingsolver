// The visualizer of a solution ('solution.js'), in a container of a page:
// - for the problem types in two and three dimensions, an overview of all the
//   bins (a grid, with the number of copies of each bin), and below it a view
//   of one bin, changed with the arrows (buttons, or the left and right keys);
//   clicking a bin of the overview shows it below, and the bin shown is
//   highlighted in the overview; all the bins are at the same scale, in the
//   overview and in the view of a bin; box and boxstacks are drawn in
//   isometric views, the view of a bin turned with its "Rotate" button (or the
//   R key);
// - for onedimensional, a row for each bin.

import {numberOfBins} from "./solution.js";
import {binsFrame, renderBin, renderRows} from "./render.js";
import {binsFrame3, renderBin3} from "./render3d.js";

function element(name, className, text = null) {
    const e = document.createElement(name);
    if (className)
        e.className = className;
    if (text !== null)
        e.textContent = text;
    return e;
}

function binsSummary(solution) {
    const total = numberOfBins(solution);
    const different = solution.bins.length;
    return `${total} bin${(total !== 1)? "s": ""}`
        + ((different !== total)? `: ${different} different bin${(different !== 1)? "s": ""}`: "");
}

// Show a solution in 'container' (its content is replaced). 'selected': the
// index of the bin shown first. Returns '{select(index), selected()}'.
export function createViewer(container, solution, {selected = 0} = {}) {
    container.replaceChildren();
    container.classList.add("viewer");
    const summary = element("p", "viewer-summary", binsSummary(solution));
    container.appendChild(summary);
    if (solution.problemType === "onedimensional") {
        container.appendChild(renderRows(solution));
        return {select: () => {}, selected: () => 0};
    }

    const bins = solution.bins;
    let current = Math.min(Math.max(selected, 0), bins.length - 1);
    // All the bins at the same scale: in a frame of the size of the largest
    // one. box and boxstacks: an isometric view, seen from one of the four
    // corners of the bin ('rotation', changed in the view of a bin).
    const threeDimensional = (solution.dimensions === 3);
    const frame = threeDimensional? binsFrame3(bins): binsFrame(bins);
    let rotation = 0;
    const draw = (bin, options) => threeDimensional?
        renderBin3(bin, {frame, ...options}):
        renderBin(bin, {frame, ...options});

    // The overview: a button for each bin.
    const overview = element("div", "viewer-overview");
    overview.setAttribute("role", "group");
    overview.setAttribute("aria-label", "All the bins");
    const cells = bins.map((bin, index) => {
        const cell = element("button", "viewer-cell");
        cell.type = "button";
        cell.title = `Show bin ${bin.index}`;
        cell.appendChild(draw(bin, {}));
        const caption = element("span", "viewer-cell-caption", `Bin ${bin.index}`);
        if (bin.copies > 1)
            caption.append(" ", element("span", "viewer-copies", `×${bin.copies}`));
        cell.appendChild(caption);
        cell.addEventListener("click", () => select(index));
        overview.appendChild(cell);
        return cell;
    });

    // The view of one bin.
    const detail = element("div", "viewer-detail");
    const header = element("div", "viewer-detail-header");
    const previous = element("button", "viewer-previous", "←");
    previous.type = "button";
    previous.title = "Previous bin (left arrow)";
    previous.setAttribute("aria-label", "Previous bin");
    const next = element("button", "viewer-next", "→");
    next.type = "button";
    next.title = "Next bin (right arrow)";
    next.setAttribute("aria-label", "Next bin");
    const title = element("span", "viewer-detail-title");
    title.setAttribute("aria-live", "polite");
    header.append(previous, title, next);
    if (threeDimensional) {
        // The bin seen from the next corner: the items hidden behind other
        // items become visible.
        const rotate = element("button", "viewer-rotate", "Rotate");
        rotate.type = "button";
        rotate.title = "See the bin from another corner (R)";
        rotate.addEventListener("click", () => {
            rotation = (rotation + 1) % 4;
            select(current);
        });
        header.appendChild(rotate);
    }
    const image = element("div", "viewer-detail-image");
    detail.append(header, image);
    previous.addEventListener("click", () => select(current - 1));
    next.addEventListener("click", () => select(current + 1));

    container.append(overview, detail);
    // The left and right keys, in the viewer.
    container.tabIndex = -1;
    container.addEventListener("keydown", (event) => {
        if (event.target.closest("input, select, textarea") !== null)
            return;
        if (event.key === "ArrowLeft") {
            select(current - 1);
            event.preventDefault();
        } else if (event.key === "ArrowRight") {
            select(current + 1);
            event.preventDefault();
        } else if (threeDimensional && (event.key === "r" || event.key === "R")
                && !event.ctrlKey && !event.metaKey && !event.altKey) {
            rotation = (rotation + 1) % 4;
            select(current);
            event.preventDefault();
        }
    });

    function select(index) {
        if (index < 0 || index >= bins.length)
            return;
        current = index;
        const bin = bins[index];
        cells.forEach((cell, i) => cell.setAttribute("aria-pressed", String(i === index)));
        // The bin highlighted in the overview kept visible, scrolling the
        // overview only (not the page).
        const cell = cells[index];
        if (cell.offsetTop < overview.scrollTop)
            overview.scrollTop = cell.offsetTop;
        else if (cell.offsetTop + cell.offsetHeight > overview.scrollTop + overview.clientHeight)
            overview.scrollTop = cell.offsetTop + cell.offsetHeight - overview.clientHeight;
        let text = `Bin ${bin.index} (${index + 1} / ${bins.length})`;
        if (bin.copies > 1)
            text += `, used ${bin.copies} times`;
        if (bin.binTypeId !== null && bin.binTypeId !== undefined)
            text += ` · bin type ${bin.binTypeId}`;
        text += ` · ${bin.items.length} item${(bin.items.length !== 1)? "s": ""}`;
        title.textContent = text;
        previous.disabled = (index === 0);
        next.disabled = (index === bins.length - 1);
        image.replaceChildren(draw(bin, {detail: true, rotation}));
    }

    if (bins.length > 0)
        select(current);
    return {select, selected: () => current};
}
