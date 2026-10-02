// LayerTime - the operator interface on the wrist. Connect IQ Device App for
// the Garmin tactix 7 AMOLED (epix2pro51mm).
//
// Copyright (C) 2026 Michael Van Geertruy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import Toybox.Graphics;
import Toybox.Lang;
import Toybox.Math;
import Toybox.Weather;

// Small vector glyphs for the home screen, drawn with the graphics
// primitives so they scale with the display and need no bitmap assets.
// (cx, cy) is the glyph centre; r is its half-size in pixels.
module Icons {

    function satellite(dc as Dc, cx as Number, cy as Number, r as Number, color as Number) as Void {
        dc.setColor(color, Graphics.COLOR_TRANSPARENT);
        // Body: a small square turned 45 degrees, with a panel each side.
        var b = (r * 0.35).toNumber();
        dc.fillPolygon([[cx, cy - b], [cx + b, cy], [cx, cy + b], [cx - b, cy]]);
        var p = (r * 0.9).toNumber();
        var q = (r * 0.45).toNumber();
        dc.fillPolygon([[cx - p, cy - q], [cx - q, cy - p], [cx - q + b, cy - p + b], [cx - p + b, cy - q + b]]);
        dc.fillPolygon([[cx + p, cy + q], [cx + q, cy + p], [cx + q - b, cy + p - b], [cx + p - b, cy + q - b]]);
        // Signal arcs toward the lower left.
        dc.setPenWidth(2);
        dc.drawArc(cx + (r * 0.2).toNumber(), cy + (r * 0.2).toNumber(), (r * 0.8).toNumber(), Graphics.ARC_COUNTER_CLOCKWISE, 200, 250);
        dc.drawArc(cx + (r * 0.2).toNumber(), cy + (r * 0.2).toNumber(), (r * 1.2).toNumber(), Graphics.ARC_COUNTER_CLOCKWISE, 200, 250);
        dc.setPenWidth(1);
    }

    function antenna(dc as Dc, cx as Number, cy as Number, r as Number, color as Number) as Void {
        dc.setColor(color, Graphics.COLOR_TRANSPARENT);
        // Mast: a tall thin triangle on a base.
        dc.fillPolygon([[cx, cy - (r * 0.5).toNumber()], [cx + (r * 0.45).toNumber(), cy + r], [cx - (r * 0.45).toNumber(), cy + r]]);
        dc.fillCircle(cx, cy - (r * 0.5).toNumber(), (r * 0.18).toNumber() + 1);
        // Waves either side of the tip.
        dc.setPenWidth(2);
        var ty = cy - (r * 0.5).toNumber();
        dc.drawArc(cx, ty, (r * 0.55).toNumber(), Graphics.ARC_COUNTER_CLOCKWISE, 130, 230);
        dc.drawArc(cx, ty, (r * 0.95).toNumber(), Graphics.ARC_COUNTER_CLOCKWISE, 130, 230);
        dc.drawArc(cx, ty, (r * 0.55).toNumber(), Graphics.ARC_COUNTER_CLOCKWISE, 310, 50);
        dc.drawArc(cx, ty, (r * 0.95).toNumber(), Graphics.ARC_COUNTER_CLOCKWISE, 310, 50);
        dc.setPenWidth(1);
    }

    // Sun, cloud, rain, snow or an outline circle for no data.
    function weather(dc as Dc, cx as Number, cy as Number, r as Number, condition as Number?, color as Number) as Void {
        dc.setColor(color, Graphics.COLOR_TRANSPARENT);
        if (condition == null) {
            dc.setPenWidth(2);
            dc.drawCircle(cx, cy, r);
            dc.setPenWidth(1);
            return;
        }
        var kind = classify(condition);
        if (kind == 0) {
            dc.fillCircle(cx, cy, (r * 0.5).toNumber());
            dc.setPenWidth(2);
            for (var a = 0; a < 360; a += 45) {
                var rad = Math.toRadians(a);
                dc.drawLine((cx + r * 0.7 * Math.cos(rad)).toNumber(), (cy - r * 0.7 * Math.sin(rad)).toNumber(),
                            (cx + r * Math.cos(rad)).toNumber(), (cy - r * Math.sin(rad)).toNumber());
            }
            dc.setPenWidth(1);
            return;
        }
        // Cloud: two lobes over a bar.
        dc.fillCircle(cx - (r * 0.35).toNumber(), cy, (r * 0.45).toNumber());
        dc.fillCircle(cx + (r * 0.2).toNumber(), cy - (r * 0.2).toNumber(), (r * 0.55).toNumber());
        dc.fillRectangle(cx - (r * 0.7).toNumber(), cy, (r * 1.5).toNumber(), (r * 0.45).toNumber());
        if (kind == 2) {
            dc.setPenWidth(2);
            for (var i = -1; i <= 1; i++) {
                var x = cx + i * (r * 0.4).toNumber();
                dc.drawLine(x, cy + (r * 0.6).toNumber(), x - 2, cy + r + 2);
            }
            dc.setPenWidth(1);
        } else if (kind == 3) {
            for (var i = -1; i <= 1; i++) {
                dc.fillCircle(cx + i * (r * 0.4).toNumber(), cy + (r * 0.8).toNumber(), 2);
            }
        }
    }

    // 0 clear, 1 cloud, 2 rain, 3 snow.
    function classify(c as Number) as Number {
        if (c == Weather.CONDITION_CLEAR || c == Weather.CONDITION_MOSTLY_CLEAR || c == Weather.CONDITION_FAIR ||
            c == Weather.CONDITION_PARTLY_CLEAR) { return 0; }
        if (c == Weather.CONDITION_SNOW || c == Weather.CONDITION_LIGHT_SNOW || c == Weather.CONDITION_HEAVY_SNOW ||
            c == Weather.CONDITION_FLURRIES || c == Weather.CONDITION_CHANCE_OF_SNOW || c == Weather.CONDITION_CLOUDY_CHANCE_OF_SNOW ||
            c == Weather.CONDITION_WINTRY_MIX || c == Weather.CONDITION_ICE_SNOW || c == Weather.CONDITION_SLEET) { return 3; }
        if (c == Weather.CONDITION_RAIN || c == Weather.CONDITION_LIGHT_RAIN || c == Weather.CONDITION_HEAVY_RAIN ||
            c == Weather.CONDITION_SHOWERS || c == Weather.CONDITION_LIGHT_SHOWERS || c == Weather.CONDITION_HEAVY_SHOWERS ||
            c == Weather.CONDITION_SCATTERED_SHOWERS || c == Weather.CONDITION_CHANCE_OF_SHOWERS || c == Weather.CONDITION_DRIZZLE ||
            c == Weather.CONDITION_THUNDERSTORMS || c == Weather.CONDITION_SCATTERED_THUNDERSTORMS ||
            c == Weather.CONDITION_CHANCE_OF_THUNDERSTORMS || c == Weather.CONDITION_CLOUDY_CHANCE_OF_RAIN ||
            c == Weather.CONDITION_RAIN_SNOW || c == Weather.CONDITION_FREEZING_RAIN || c == Weather.CONDITION_HAIL) { return 2; }
        return 1;
    }

    // Half sun on a horizon with rays; an arrow marks rise (up) or set (down).
    function solar(dc as Dc, cx as Number, cy as Number, r as Number, rise as Boolean, color as Number) as Void {
        dc.setColor(color, Graphics.COLOR_TRANSPARENT);
        var hy = cy + (r * 0.3).toNumber();
        dc.setPenWidth(2);
        dc.drawArc(cx, hy, (r * 0.55).toNumber(), Graphics.ARC_COUNTER_CLOCKWISE, 0, 180);
        dc.drawLine(cx - r, hy, cx + r, hy);
        for (var a = 30; a <= 150; a += 40) {
            var rad = Math.toRadians(a);
            dc.drawLine((cx + r * 0.75 * Math.cos(rad)).toNumber(), (hy - r * 0.75 * Math.sin(rad)).toNumber(),
                        (cx + r * Math.cos(rad)).toNumber(), (hy - r * Math.sin(rad)).toNumber());
        }
        dc.setPenWidth(1);
        var ax = cx + r + 4;
        if (rise) {
            dc.fillPolygon([[ax, hy - (r * 0.6).toNumber()], [ax - 3, hy - (r * 0.2).toNumber()], [ax + 3, hy - (r * 0.2).toNumber()]]);
        } else {
            dc.fillPolygon([[ax, hy - (r * 0.2).toNumber()], [ax - 3, hy - (r * 0.6).toNumber()], [ax + 3, hy - (r * 0.6).toNumber()]]);
        }
    }

    // A folded map: three panels with zigzag folds.
    function map(dc as Dc, cx as Number, cy as Number, r as Number, color as Number) as Void {
        dc.setColor(color, Graphics.COLOR_TRANSPARENT);
        dc.setPenWidth(2);
        var w = (r * 1.4).toNumber();
        var h = r;
        var x0 = cx - w;
        var third = (2 * w / 3).toNumber();
        dc.drawLine(x0, cy - h, x0 + third, cy - h + 3);
        dc.drawLine(x0 + third, cy - h + 3, x0 + 2 * third, cy - h);
        dc.drawLine(x0 + 2 * third, cy - h, x0 + 2 * w, cy - h + 3);
        dc.drawLine(x0, cy + h, x0 + third, cy + h - 3);
        dc.drawLine(x0 + third, cy + h - 3, x0 + 2 * third, cy + h);
        dc.drawLine(x0 + 2 * third, cy + h, x0 + 2 * w, cy + h - 3);
        dc.drawLine(x0, cy - h, x0, cy + h);
        dc.drawLine(x0 + third, cy - h + 3, x0 + third, cy + h - 3);
        dc.drawLine(x0 + 2 * third, cy - h, x0 + 2 * third, cy + h);
        dc.drawLine(x0 + 2 * w, cy - h + 3, x0 + 2 * w, cy + h - 3);
        dc.setPenWidth(1);
    }

    // Three nodes joined in a triangle.
    function mesh(dc as Dc, cx as Number, cy as Number, r as Number, color as Number) as Void {
        dc.setColor(color, Graphics.COLOR_TRANSPARENT);
        var top = [cx, cy - r];
        var left = [cx - r, cy + (r * 0.7).toNumber()];
        var right = [cx + r, cy + (r * 0.7).toNumber()];
        dc.setPenWidth(2);
        dc.drawLine(top[0], top[1], left[0], left[1]);
        dc.drawLine(top[0], top[1], right[0], right[1]);
        dc.drawLine(left[0], left[1], right[0], right[1]);
        dc.setPenWidth(1);
        var n = (r * 0.3).toNumber() + 1;
        dc.fillCircle(top[0], top[1], n);
        dc.fillCircle(left[0], left[1], n);
        dc.fillCircle(right[0], right[1], n);
    }
}
