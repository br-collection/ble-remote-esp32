// Widget view and command menu
import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;

class MainView extends WatchUi.View {
    function initialize() {
        View.initialize();
    }

    function onUpdate(dc as Graphics.Dc) as Void {
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_BLACK);
        dc.clear();
        var cx = dc.getWidth() / 2;
        var cy = dc.getHeight() / 2;
        dc.drawText(cx, cy - dc.getFontHeight(Graphics.FONT_MEDIUM) - 4,
                    Graphics.FONT_MEDIUM, str(Rez.Strings.AppName),
                    Graphics.TEXT_JUSTIFY_CENTER);
        dc.drawText(cx, cy + 2, Graphics.FONT_XTINY, getApp().statusText(),
                    Graphics.TEXT_JUSTIFY_CENTER);
    }
}

class MainDelegate extends WatchUi.BehaviorDelegate {
    function initialize() {
        BehaviorDelegate.initialize();
    }

    // START button or tap
    function onSelect() as Boolean {
        openCommandMenu();
        return true;
    }
}

function openCommandMenu() as Void {
    var app = getApp();
    var cfg = app.config;
    if (!app.isReady() || cfg == null) {
        WatchUi.requestUpdate();   // the view shows the reason
        return;
    }
    var c = app.client;
    if (c != null) {
        c.preconnect();   // extend the keep time or reconnect
    }
    var menu = new WatchUi.Menu2({ :title => str(Rez.Strings.AppName) });
    for (var i = 0; i < 4; i++) {
        if (cfg.enabled[i]) {
            menu.addItem(new WatchUi.MenuItem(cfg.names[i], null, i + 1, null));
        }
    }
    WatchUi.pushView(menu, new CommandMenuDelegate(), WatchUi.SLIDE_LEFT);
}

class CommandMenuDelegate extends WatchUi.Menu2InputDelegate {
    function initialize() {
        Menu2InputDelegate.initialize();
    }

    function onSelect(item as WatchUi.MenuItem) as Void {
        var ctrl = new SendController(item.getId() as Number);
        ctrl.start();
    }
}
