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
import Toybox.WatchUi;

// Pages reached from the home screen that are not built yet. Each says so
// on the watch rather than pretending: MAPPING (the Garmin's mapping page,
// separation of duties: the watch owns mapping) and the two mesh user
// interfaces (Meshtastic and MeshCore, reached through the phone). BACK
// returns home.
class PlaceholderView extends WatchUi.View {

    private var _title as String;
    private var _lines as Array<String>;

    public function initialize(title as String, lines as Array<String>) {
        View.initialize();
        _title = title;
        _lines = lines;
    }

    public function onUpdate(dc as Dc) as Void {
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_BLACK);
        dc.clear();
        var cx = dc.getWidth() / 2;
        var h = dc.getHeight();
        dc.setColor(0x30D0FF, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.22, Graphics.FONT_MEDIUM, _title, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        var y = h * 0.40;
        for (var i = 0; i < _lines.size(); i++) {
            dc.drawText(cx, y, Graphics.FONT_SMALL, _lines[i], Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            y += dc.getFontHeight(Graphics.FONT_SMALL) + 4;
        }
        dc.setColor(0x8C8C8C, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.84, Graphics.FONT_XTINY, "BACK returns home.", Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }
}

// The MESH page: a native menu with the two mesh networks LayerTime carries.
class MeshMenuDelegate extends WatchUi.Menu2InputDelegate {
    public function initialize() { Menu2InputDelegate.initialize(); }

    public function onSelect(item as MenuItem) as Void {
        var id = item.getId();
        var name = id == :meshcore ? "MESHCORE" : "MESHTASTIC";
        WatchUi.pushView(new PlaceholderView(name, ["Mesh user interface:", "not in this increment.", "Radio and protocol run", "on the phone."]),
                         new WatchUi.BehaviorDelegate(), WatchUi.SLIDE_LEFT);
    }

    public function onBack() as Void {
        WatchUi.popView(WatchUi.SLIDE_RIGHT);
    }
}

module Pages {
    function openMapping() as Void {
        WatchUi.pushView(new PlaceholderView("MAPPING", ["Mapping page:", "not in this increment."]),
                         new WatchUi.BehaviorDelegate(), WatchUi.SLIDE_LEFT);
    }

    function openMesh() as Void {
        var menu = new WatchUi.Menu2({:title => "MESH"});
        menu.addItem(new WatchUi.MenuItem("Meshtastic", "via phone", :meshtastic, null));
        menu.addItem(new WatchUi.MenuItem("MeshCore", "via phone", :meshcore, null));
        WatchUi.pushView(menu, new MeshMenuDelegate(), WatchUi.SLIDE_LEFT);
    }

    // The Recon page (ReconPage.mc). The Link diagnostics page is under its
    // Controls.
    function openRecon(link as LinkClient) as Void {
        var view = new $.ReconView(link);
        WatchUi.pushView(view, new $.ReconDelegate(link, view), WatchUi.SLIDE_UP);
    }
}
