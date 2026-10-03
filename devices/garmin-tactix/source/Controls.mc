// LayerTime - passive early-warning system. Connect IQ Device App for the
// Garmin tactix 8 AMOLED.
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

// The Recon Controls: a native menu of the commands the LayerTime Link
// binds (contracts/link.md, COMMAND). Each sends one COMMAND; the result
// comes back as a toast (showResult, LinkClient.resultObserver). Clear asks
// for confirmation first. Link diagnostics (the Increment 1 transport page)
// is the last item.
module Controls {

    function open(link as LinkClient) as Void {
        var alert = (link.flags & Link.FLAG_ALERT_PENDING) != 0;
        var menu = new WatchUi.Menu2({:title => "CONTROLS"});
        menu.addItem(new WatchUi.MenuItem("Start Recon", ReconNames.shortName(link.selected != 0 ? link.selected : 1), :start, null));
        menu.addItem(new WatchUi.MenuItem("Stop Recon", "leave manual Recon", :stop, null));
        menu.addItem(new WatchUi.ToggleMenuItem("Early warning", null, :earlyWarning,
                                                (link.flags & Link.FLAG_EARLY_WARNING_ENABLED) != 0, null));
        menu.addItem(new WatchUi.ToggleMenuItem("Sleep mode", "alerts off, still logged", :sleep,
                                                (link.flags & Link.FLAG_SLEEP_MODE) != 0, null));
        menu.addItem(new WatchUi.MenuItem("Acknowledge alert", alert ? "alert pending" : "no alert", :ack, null));
        menu.addItem(new WatchUi.MenuItem("Clear detections", "on LayerWand", :clear, null));
        menu.addItem(new WatchUi.MenuItem("Link diagnostics", null, :diagnostics, null));
        WatchUi.pushView(menu, new ControlsDelegate(link), WatchUi.SLIDE_UP);
    }

    // The targets ReconStart accepts: 1 (All) to 16 (GoogleTag).
    function openTargets(link as LinkClient) as Void {
        var menu = new WatchUi.Menu2({:title => "START RECON"});
        for (var t = 1; t <= 16; t++) {
            menu.addItem(new WatchUi.MenuItem(ReconNames.displayName(t), null, t, null));
        }
        WatchUi.pushView(menu, new TargetDelegate(link), WatchUi.SLIDE_LEFT);
    }

    function send(link as LinkClient, commandType as Number, arg as Number?) as Void {
        if (!link.sendCommand(commandType, arg)) { toast("LayerWand not connected"); }
    }

    // LinkClient.resultObserver: one toast per command.
    function showResult(commandType as Number, result as Number) as Void {
        if (result == -1) { toast("No answer from LayerWand"); return; }
        if (result != Link.RESULT_OK) { toast(Link.commandResultName(result)); return; }
        if (commandType == Link.CMD_RECON_START) { toast("Recon started"); }
        else if (commandType == Link.CMD_RECON_STOP) { toast("Recon stopped"); }
        else if (commandType == Link.CMD_RECON_CLEAR_EVENTS) { toast("Detections cleared"); }
        else if (commandType == Link.CMD_RECON_ACKNOWLEDGE_ALERT) { toast("Alert acknowledged"); }
        else if (commandType == Link.CMD_SET_SLEEP_MODE) { toast("Sleep mode set"); }
        else if (commandType == Link.CMD_SET_EARLY_WARNING) { toast("Early warning set"); }
        WatchUi.requestUpdate();
    }

    // LinkClient.gapObserver: some detections were dropped on LayerWand
    // before the watch fetched them. The Recon page keeps showing GAP.
    function showGap() as Void {
        toast("Some detections were missed.");
        WatchUi.requestUpdate();
    }

    // LinkClient.noSdObserver: the LayerWand has no SD card log, so its
    // events live only in its memory. Shown once per connection.
    function showNoSdCard() as Void {
        WatchUi.pushView(new $.NoticeView("NO SD CARD", NO_SD_CARD_TEXT), new $.NoticeDelegate(), WatchUi.SLIDE_UP);
    }

    // Michael's wording (2026-10-03).
    const NO_SD_CARD_TEXT = "No SD card is installed. Limited memory will result in errors when the memory is full.";

    function toast(text as String) as Void {
        if (WatchUi has :showToast) { WatchUi.showToast(text, null); }
    }
}

class ControlsDelegate extends WatchUi.Menu2InputDelegate {

    private var _link as LinkClient;

    public function initialize(link as LinkClient) {
        Menu2InputDelegate.initialize();
        _link = link;
    }

    public function onSelect(item as MenuItem) as Void {
        var id = item.getId();
        if (id == :start) {
            Controls.openTargets(_link);
        } else if (id == :stop) {
            Controls.send(_link, Link.CMD_RECON_STOP, null);
        } else if (id == :earlyWarning) {
            Controls.send(_link, Link.CMD_SET_EARLY_WARNING, (item as ToggleMenuItem).isEnabled() ? 1 : 0);
        } else if (id == :sleep) {
            Controls.send(_link, Link.CMD_SET_SLEEP_MODE, (item as ToggleMenuItem).isEnabled() ? 1 : 0);
        } else if (id == :ack) {
            Controls.send(_link, Link.CMD_RECON_ACKNOWLEDGE_ALERT, null);
        } else if (id == :clear) {
            WatchUi.pushView(new WatchUi.Confirmation("Clear all detections on LayerWand?"),
                             new ClearConfirmDelegate(_link), WatchUi.SLIDE_IMMEDIATE);
        } else if (id == :diagnostics) {
            var diag = new $.LayerTimeView(_link);
            WatchUi.pushView(diag, new $.LayerTimeDelegate(_link, diag), WatchUi.SLIDE_LEFT);
        }
    }

    public function onBack() as Void { WatchUi.popView(WatchUi.SLIDE_DOWN); }
}

class TargetDelegate extends WatchUi.Menu2InputDelegate {

    private var _link as LinkClient;

    public function initialize(link as LinkClient) {
        Menu2InputDelegate.initialize();
        _link = link;
    }

    public function onSelect(item as MenuItem) as Void {
        Controls.send(_link, Link.CMD_RECON_START, item.getId() as Number);
        WatchUi.popView(WatchUi.SLIDE_RIGHT);
    }

    public function onBack() as Void { WatchUi.popView(WatchUi.SLIDE_RIGHT); }
}

class ClearConfirmDelegate extends WatchUi.ConfirmationDelegate {

    private var _link as LinkClient;

    public function initialize(link as LinkClient) {
        ConfirmationDelegate.initialize();
        _link = link;
    }

    public function onResponse(response as Confirm) as Boolean {
        if (response == WatchUi.CONFIRM_YES) { Controls.send(_link, Link.CMD_RECON_CLEAR_EVENTS, null); }
        return true;
    }
}
