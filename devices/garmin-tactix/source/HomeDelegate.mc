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

import Toybox.Lang;
import Toybox.WatchUi;

// Home-screen input. Physical keys: UP and DOWN move the focus between the
// Recon entry, MAPPING and MESH; START opens the focused one (or clears a
// new-session notice); MENU opens the Recon Controls; BACK is left to the
// system and exits the app. Touch: a tap on the Recon entry, MAPPING or MESH
// opens that control; on the preview build only, a tap on the LAYERTIME
// title moves to the next preview scenario (Preview.mc); a tap anywhere else
// does nothing.
//
// Why START is handled in onKey and not onSelect: on the tactix 7 AMOLED
// both the START key and a screen tap are the Select behavior (the device
// file maps gesture "tap" to onSelect), and a BehaviorDelegate that returns
// true from a behavior suppresses the matching input callback. Handling
// Select there made every tap open the focused entry, the Recon page by
// default, and onTap never ran. onSelect therefore declines, so a tap
// reaches onTap and START reaches onKey.
class HomeDelegate extends WatchUi.BehaviorDelegate {

    private var _link as LinkClient;
    private var _view as HomeView;

    public function initialize(link as LinkClient, view as HomeView) {
        BehaviorDelegate.initialize();
        _link = link;
        _view = view;
    }

    private function open(which as Number) as Void {
        if (which == Focus.MAPPING) { Pages.openMapping(); }
        else if (which == Focus.MESH) { Pages.openMesh(); }
        else { Pages.openRecon(_link); }
    }

    public function onSelect() as Boolean {
        return false; // a tap goes on to onTap, START to onKey
    }

    public function onKey(evt as KeyEvent) as Boolean {
        if (evt.getKey() != WatchUi.KEY_ENTER) { return false; }
        if (_link.newSession) {
            _link.acknowledgeNewSession();
            WatchUi.requestUpdate();
            return true;
        }
        open(_view.focus);
        return true;
    }

    public function onNextPage() as Boolean {
        _view.focus = (_view.focus + 1) % 3;
        WatchUi.requestUpdate();
        return true;
    }

    public function onPreviousPage() as Boolean {
        _view.focus = (_view.focus + 2) % 3;
        WatchUi.requestUpdate();
        return true;
    }

    public function onMenu() as Boolean {
        Controls.open(_link);
        return true;
    }

    // Every tap is consumed here: a control opens, the title changes the
    // preview scenario (preview build only), and anything else does nothing.
    public function onTap(evt as ClickEvent) as Boolean {
        var xy = evt.getCoordinates();
        var target = _view.hitTarget(xy[0], xy[1]);
        if (target == :title) {
            Env.titleTapped(_link); // a no-op outside the preview build
            return true;
        }
        var which = target == :recon ? Focus.RECON : (target == :mapping ? Focus.MAPPING : (target == :mesh ? Focus.MESH : -1));
        if (which < 0) { return true; }
        _view.focus = which;
        if (_link.newSession) { _link.acknowledgeNewSession(); }
        open(which);
        return true;
    }
}
