// SPDX-License-Identifier: MIT
// Shared, side-effect-free logic. Used by QML and the Node.js regression suite.

function cleanUrl(value) {
    const raw = String(value || "").split("?")[0].trim();
    // KWin and launchers can disagree about the slash after applications:.
    if (raw.startsWith("applications:/"))
        return "applications:" + raw.slice("applications:".length).replace(/^\/+/, "");
    return raw;
}

function zoomAt(index, hoverPosition, zoomScale, zoomSpread) {
    if (!Number.isFinite(index) || !Number.isFinite(hoverPosition) || hoverPosition < 0)
        return 1;
    const scale = Math.max(1, Math.min(3, Number(zoomScale) || 1));
    const spread = Math.max(0, Math.min(8, Number(zoomSpread) || 0));
    const radius = spread + 0.5;
    const d = Math.abs(index - hoverPosition);
    if (d >= radius) return 1;
    const factor = (1 + Math.cos(Math.PI * d / radius)) / 2;
    return 1 + (scale - 1) * factor;
}

function reorderLaunchers(launchers, source, target, after) {
    const copy = Array.from(launchers || []);
    const fromId = cleanUrl(source);
    const toId = cleanUrl(target);
    if (!fromId || !toId || fromId === toId) return copy;
    const fromIndex = copy.findIndex(url => cleanUrl(url) === fromId);
    if (fromIndex < 0 || !copy.some(url => cleanUrl(url) === toId)) return copy;
    const moved = copy.splice(fromIndex, 1)[0];
    const targetIndex = copy.findIndex(url => cleanUrl(url) === toId);
    copy.splice(targetIndex + (after ? 1 : 0), 0, moved);
    return copy;
}

function chooseDropTarget(pinnedCells, pointerX) {
    if (!pinnedCells || !pinnedCells.length || !Number.isFinite(pointerX)) return null;
    let best = null;
    let minDistance = Infinity;
    for (const cell of pinnedCells) {
        if (!cell || !cell.url || !Number.isFinite(cell.left) || !Number.isFinite(cell.right)) continue;
        const center = (cell.left + cell.right) / 2;
        const delta = Math.abs(pointerX - center);
        if (delta < minDistance) {
            minDistance = delta;
            const after = pointerX >= center;
            best = { url: cell.url, after: after, guideX: after ? cell.right : cell.left };
        }
    }
    return best;
}

// Stable geometry: icon slots and the Wayland window NEVER resize under hover.
// Extra pixel widths are distributed symmetrically, shifting neighbouring icons.
function totalZoomExtra(count, hoverPosition, zoomScale, zoomSpread, iconSize) {
    let extra = 0;
    for (let i = 0; i < count; ++i)
        extra += (zoomAt(i, hoverPosition, zoomScale, zoomSpread) - 1) * iconSize;
    return extra;
}

function visualShift(index, count, hoverPosition, zoomScale, zoomSpread, iconSize) {
    if (index < 0 || index >= count || hoverPosition < 0) return 0;
    let leftExtra = 0;
    for (let i = 0; i < index; ++i)
        leftExtra += (zoomAt(i, hoverPosition, zoomScale, zoomSpread) - 1) * iconSize;
    const ownExtra = (zoomAt(index, hoverPosition, zoomScale, zoomSpread) - 1) * iconSize;
    return leftExtra + ownExtra / 2 - totalZoomExtra(count, hoverPosition, zoomScale, zoomSpread, iconSize) / 2;
}

function lastWindowIndex(windows) {
    // Prefer the most recently used window (macOS click behaviour).
    // Falling back to first visible window avoids activating a group parent.
    if (!windows || !windows.length) return -1;
    let best = -1;
    let last = -Infinity;
    for (let i = 0; i < windows.length; ++i) {
        const w = windows[i];
        const stamp = Number.isFinite(w.lastUsed) ? w.lastUsed : 0;
        if (w.active) return i;
        if (w.minimized) continue;
        if (stamp > last) { best = i; last = stamp; }
    }
    if (best >= 0) return best;
    for (let i = 0; i < windows.length; ++i)
        if (Number.isFinite(windows[i].lastUsed) && windows[i].lastUsed > last) {
            best = i; last = windows[i].lastUsed;
        }
    return best >= 0 ? best : 0;
}

function compactTitle(title, maximum) {
    const text = String(title || "").replace(/\s+/g, " ").trim();
    const limit = Math.max(8, Number(maximum) || 48);
    return text.length > limit ? text.slice(0, limit - 1) + "…" : text;
}

// One pass per pointer event, rather than re-running O(N) calculations for
// every icon delegate; return arrays consumed by GPU transform properties.
function zoomLayout(count, hoverPosition, zoomScale, zoomSpread, iconSize) {
    const scales = [];
    const shifts = [];
    let totalExtra = 0;
    for (let i = 0; i < count; i++) {
        const scale = zoomAt(i, hoverPosition, zoomScale, zoomSpread);
        scales.push(scale);
        totalExtra += (scale - 1) * iconSize;
    }
    let prefix = 0;
    for (let i = 0; i < count; i++) {
        const ownExtra = (scales[i] - 1) * iconSize;
        shifts.push(prefix + ownExtra / 2 - totalExtra / 2);
        prefix += ownExtra;
    }
    return {scales: scales, shifts: shifts, extra: totalExtra};
}

// Conservative envelope only recomputed when settings or task counts change.
// It prevents a Wayland input-mask update on every animation frame.
function maximumZoomExtra(count, zoomScale, zoomSpread, iconSize) {
    const spread = Math.max(0, Math.min(8, Number(zoomSpread) || 0));
    const scale = Math.max(1, Math.min(3, Number(zoomScale) || 1));
    return Math.max(0, Math.min(count, 2 * spread + 2)) * (scale - 1) * iconSize;
}

// Cursor position interpolates between FIXED icon slot centres (including the
// wider gap occupied by the separator), so crossing slots never snaps zoom.
function hoverBetweenCenters(centers, pointerX, slotWidth) {
    if (!centers || centers.length === 0 || !Number.isFinite(pointerX)) return -1;
    const radius = Math.max(1, slotWidth) / 2 + 8;
    if (pointerX < centers[0] - radius || pointerX > centers[centers.length - 1] + radius)
        return -1;
    if (pointerX <= centers[0]) return 0;
    for (let i = 1; i < centers.length; i++) {
        if (pointerX <= centers[i]) {
            const width = Math.max(1, centers[i] - centers[i - 1]);
            return (i - 1) + (pointerX - centers[i - 1]) / width;
        }
    }
    return centers.length - 1;
}

// Actual separator position is the boundary between scaled icon slots.
// It must follow the same continuous displacement as the neighbours.
function dividerShift(scales, pinnedCount, iconSize) {
    if (!scales || pinnedCount <= 0 || pinnedCount >= scales.length) return 0;
    let total = 0;
    let left = 0;
    for (let i = 0; i < scales.length; ++i) {
        const extra = (scales[i] - 1) * iconSize;
        total += extra;
        if (i < pinnedCount) left += extra;
    }
    return left - total / 2;
}

function rectanglesOverlap(a, b) {
    if (!a || !b || a.width <= 0 || a.height <= 0 || b.width <= 0 || b.height <= 0)
        return false;
    return a.x < b.x + b.width && a.x + a.width > b.x &&
           a.y < b.y + b.height && a.y + a.height > b.y;
}

// Permits dragging an unpinned running application to the pinned section.
function reorderOrPinLauncher(launchers, source, target, after) {
    const original = Array.from(launchers || []);
    const sourceId = cleanUrl(source);
    if (!sourceId) return original;
    if (original.some(url => cleanUrl(url) === sourceId))
        return reorderLaunchers(original, source, target, after);
    const targetId = cleanUrl(target);
    if (!targetId) return original;
    const targetIndex = original.findIndex(url => cleanUrl(url) === targetId);
    if (targetIndex < 0) return original;
    original.splice(targetIndex + (after ? 1 : 0), 0, source);
    return original;
}

// The drop direction must agree with the drawn guide. Previously every
// release went to the pinned-list handler, so running apps could not reorder.
function routeDrop(wasPinned, pointerX, boundary, pinnedCount, runningCount) {
    if (!Number.isFinite(pointerX)) return "cancel";
    if (wasPinned || pointerX <= boundary || runningCount <= 0) return "pin";
    return "reorder-running";
}

// The trigger region for intelligent hiding. Window and Dock coordinates
// originate from the same absolute QScreen geometry, including negative x.
function dockProximityZone(screen, dockWidth, plateHeight, gap, proximity) {
    if (!screen || !Number.isFinite(screen.width) || screen.width <= 0 ||
        !Number.isFinite(screen.height) || screen.height <= 0) return null;
    const extra = Math.max(0, proximity || 0);
    const width = Math.min(screen.width, Math.max(0, dockWidth) + extra * 2);
    const height = Math.max(1, plateHeight + gap + extra + 12);
    return {
        x: screen.x + (screen.width - width) / 2,
        y: screen.y + screen.height - height,
        width: width,
        height: height
    };
}

// Always show the same curved fan. The viewport can scroll; there is NO
// item-count threshold and NEVER a switch to a grid/thumbnail presentation.
function stackPresentation(count) {
    return "fan";
}


// Position the *surface* around an absolute local icon X. The return value
// holds the exact source X inside the clamped surface, even near screen edges.
function fanPlacement(sourceX, screenWidth, desiredWidth) {
    if (!Number.isFinite(sourceX) || !Number.isFinite(screenWidth) || screenWidth <= 0)
        return {x: 0, width: 0, anchorX: 0};
    const margin = 4;
    const width = Math.max(0, Math.min(desiredWidth, screenWidth - 2 * margin));
    const x = Math.max(margin, Math.min(screenWidth - margin - width, sourceX - width / 2));
    return {x: x, width: width, anchorX: sourceX - x};
}

// Cairo-Dock-style fan: the first leaf emerges directly from its owner.
// Later leaves move up and flare to one side on a smooth, bounded arc.
const FAN_ICON_SIZE = 34;
const FAN_ROW_HEIGHT = 41;
const FAN_ROW_STEP = 44;
const FAN_FIRST_DISTANCE = 47;
function fanLeafDistance(index) {
    return FAN_FIRST_DISTANCE + Math.max(0, index) * FAN_ROW_STEP;
}

// Keep the shape and the cascade for ALL counts. Indices passed here are
// viewport-relative so even a stack with 1,000 entries renders only visible
// delegates. The actual count is never truncated or converted to a grid.
function fanStaggerProgress(phase, index, count) {
    const p = Math.max(0, Math.min(1, Number(phase) || 0));
    const delay = Math.min(0.60, Math.max(0, index) * (count > 8 ? 0.045 : 0.092));
    const t = Math.max(0, Math.min(1, (p - delay) / (1 - delay)));
    return t * t * (3 - 2 * t);
}

function fanLabelsLeft(anchorX, panelWidth, maxLabelWidth, maxBend) {
    const curve = Number.isFinite(maxBend) ? maxBend : 0;
    return anchorX + FAN_ICON_SIZE / 2 + 10 + maxLabelWidth + curve > panelWidth - 12;
}

function fanCurveDirection(anchorX, panelWidth, maxSweep) {
    const needed = Math.max(0, maxSweep) + FAN_ICON_SIZE / 2 + 12;
    if (anchorX >= needed) return -1; // Prefer the old Cairo-Dock leftward fan.
    if (panelWidth - anchorX >= needed) return 1;
    return anchorX > panelWidth / 2 ? -1 : 1;
}

// Compact classic Cairo-Dock fan. The screenshot reference is a narrow,
// tilted stack above its base, not the broad 185px arc of beta1. A smooth
// asymptote gives a visible but modest flare while avoiding unlimited spread.
// This formula is independent of the total number of entries.
const FAN_MAX_SWEEP = 86;
const FAN_LABEL_MAX_WIDTH = 142;
const FAN_LABEL_HEIGHT = 21;
const FAN_ICON_LABEL_GAP = 3;
function fanLeafBend(index, count, limit = FAN_MAX_SWEEP) {
    const i = Math.max(0, Number(index) || 0);
    const maxSweep = Math.max(0, Number(limit) || 0);
    return maxSweep * (1 - Math.exp(-i / 8.0));
}
function fanIconTilt(index, count) {
    return Math.min(29, 24 + Math.max(0, index) * 0.7);
}
function fanLabelTilt(index, count) {
    return Math.min(24, 20 + Math.max(0, index) * 0.55);
}

// Shared geometry for both the drawn leaf and the Wayland input mask. Having
// two separate formulae allowed the hover mask to drift away from the artwork.
function fanLeafPosition(index, count, anchorX, anchorY, direction, limit,
                         labelWidth, labelsLeft, progress = 1) {
    const p = Math.max(0, Math.min(1, Number(progress) || 0));
    const bend = fanLeafBend(index, count, limit) * p;
    const centerX = anchorX + (direction < 0 ? -1 : 1) * bend;
    const centerY = anchorY - fanLeafDistance(index) * p;
    const w = FAN_ICON_SIZE + FAN_ICON_LABEL_GAP + labelWidth;
    return {
        iconCenterX: centerX, iconCenterY: centerY,
        leafX: centerX - (labelsLeft ? w - FAN_ICON_SIZE / 2 : FAN_ICON_SIZE / 2),
        leafY: centerY - FAN_ROW_HEIGHT / 2,
        width: w, height: FAN_ROW_HEIGHT,
        iconX: labelsLeft ? w - FAN_ICON_SIZE : 0,
        labelX: labelsLeft ? 0 : FAN_ICON_SIZE + FAN_ICON_LABEL_GAP,
        labelPivotX: centerX + (labelsLeft ? -1 : 1) * (FAN_ICON_SIZE / 2 + FAN_ICON_LABEL_GAP)
    };
}

// Rotated labels are interactive in their *actual transformed* bounds, not
// through an oversize invisible panel. Works for either side of the screen.
function fanLabelBounds(centerX, centerY, labelsLeft, labelWidth, tilt,
                        iconExtent = FAN_ICON_SIZE, gap = FAN_ICON_LABEL_GAP,
                        chipHeight = FAN_LABEL_HEIGHT) {
    const w = Math.max(0, Number(labelWidth) || 0);
    const theta = (labelsLeft ? 1 : -1) * Math.max(0, Number(tilt) || 0) * Math.PI / 180;
    const pivotX = centerX + (labelsLeft ? -1 : 1) * (iconExtent / 2 + gap);
    const cos = Math.cos(theta), sin = Math.sin(theta);
    const dxValues = labelsLeft ? [-w, 0] : [0, w];
    const dyValues = [-chipHeight / 2, chipHeight / 2];
    const points = [];
    for (const dx of dxValues) for (const dy of dyValues)
        points.push({x: pivotX + dx * cos - dy * sin,
                     y: centerY + dx * sin + dy * cos});
    const xs = points.map(p => p.x), ys = points.map(p => p.y);
    const x = Math.min(...xs), y = Math.min(...ys);
    return {x: x, y: y, width: Math.max(...xs) - x,
            height: Math.max(...ys) - y};
}

// Screen pixels, not the number of items, determine how many rows fit.
function fanVisibleCapacity(anchorY, top = 8) {
    if (!Number.isFinite(anchorY) || anchorY <= 0) return 1;
    // A rotated title can extend above its icon by an entire small row.
    // Reserve its maximum rise so the topmost label is not clipped.
    const titleRise = FAN_LABEL_MAX_WIDTH * Math.sin(24 * Math.PI / 180)
                      + FAN_LABEL_HEIGHT / 2 + 8;
    return Math.max(1, Math.floor((anchorY - top - titleRise - FAN_FIRST_DISTANCE)
                                  / FAN_ROW_STEP) + 1);
}

// Immutable, bounded visible window. Scrolling changes its start index while
// retaining the same *fan* layout; no whole-list delegate explosion.
function fanScrollWindow(count, capacity, requestedStart) {
    const n = Math.max(0, Math.floor(Number(count) || 0));
    const cap = Math.max(1, Math.floor(Number(capacity) || 1));
    const maxStart = Math.max(0, n - cap);
    const start = Math.max(0, Math.min(maxStart, Math.floor(Number(requestedStart) || 0)));
    return {start: start, shown: Math.min(cap, n - start), maxStart: maxStart};
}

// Hover ownership cannot be captured by the fan's wide transparent window.
// When the pointer crosses to another app, switch to its fan immediately or
// dismiss the current one. The initial fan still respects fanDelay.
function fanHoverAction(isOpen, sameOwner, availableEntries) {
    if (!isOpen || sameOwner) return "none";
    return availableEntries > 0 ? "switch" : "dismiss";
}

// Existing favorites retain order even if KDE emits a launcher-list refresh
// while an application changes from launcher to running window. Appends only
// explicitly new entries, removes only entries missing from KDE's list.
function preservePinnedOrder(previous, incoming) {
    const oldList = Array.from(previous || []);
    const newList = Array.from(incoming || []);
    const available = new Set(newList.map(cleanUrl));
    const output = [], added = new Set();
    for (const entry of oldList) {
        const id = cleanUrl(entry);
        if (id && available.has(id) && !added.has(id)) {
            output.push(entry);
            added.add(id);
        }
    }
    for (const entry of newList) {
        const id = cleanUrl(entry);
        if (id && !added.has(id)) {
            output.push(entry);
            added.add(id);
        }
    }
    return output;
}

// Keep the full icon row centered as running apps are added or removed.
function stableRowLeft(screenWidth, pinnedCount, slotWidth, spacing, totalWidth) {
    if (!Number.isFinite(screenWidth) || screenWidth <= 0) return 0;
    const left = (screenWidth - Math.max(0, totalWidth)) / 2;
    return Math.max(4, Math.min(left, Math.max(4, screenWidth - totalWidth - 4)));
}

// The pointer bridge must cover ONLY the triggering icon's column; a wide
// corridor prevents neighboring icons from receiving hover events.
function fanBridgeGeometry(anchorX, anchorY) {
    return {x: anchorX - 25, y: anchorY - 44, width: 50, height: 63};
}


// A user pin/unpin operation is computed on a copy before touching KDE's
// task source. Never synchronously mutate TasksModel from a delegate/menu.
function togglePinnedLauncher(launchers, url) {
    const target = cleanUrl(url);
    const items = Array.from(launchers || []);
    if (!target) return items;
    const index = items.findIndex(value => cleanUrl(value) === target);
    if (index < 0) return items.concat([url]);
    items.splice(index, 1);
    return items;
}
function addPinnedLauncher(launchers, url) {
    const target = cleanUrl(url);
    const items = Array.from(launchers || []);
    return !target || items.some(value => cleanUrl(value) === target) ? items : items.concat([url]);
}
function isSystemSettingsLauncher(url, appId, storageId) {
    const known = new Set(['systemsettings', 'systemsettings.desktop',
                           'org.kde.systemsettings', 'org.kde.systemsettings.desktop']);
    return [url, appId, storageId].some(value => {
        const id = cleanUrl(value).toLowerCase().replace(/^applications:/, '').replace(/^\/+/, '');
        return known.has(id);
    });
}

// Three gradually smaller hops. Starts and ends at the icon's normal Y;
// animates the icon's existing scene-graph node, not an overlay duplicate.
function launchBounceOffset(progress, size) {
    if (!Number.isFinite(progress) || progress <= 0 || progress >= 1) return 0;
    const phase = progress * 3 * Math.PI;
    const up = Math.abs(Math.sin(phase));
    if (up < 1e-8) return 0;
    return -Math.round(size * 0.42 * up * (1 - 0.45 * progress) * 100) / 100;
}
