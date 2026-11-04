// Entry point: load the configuration, prepare BLE
import Toybox.Application;
import Toybox.Lang;
import Toybox.WatchUi;

class BleRemoteApp extends Application.AppBase {

    var config as Config? = null;
    var client as BleClient? = null;
    var configError = null;            // resource of the invalid field
    var restartNeeded as Boolean = false;

    function initialize() {
        AppBase.initialize();
    }

    function onStart(state as Dictionary?) as Void {
        var cfg = new Config();
        configError = cfg.load();
        config = cfg;
        if (configError == null) {
            var c = new BleClient(cfg);
            c.setup();
            client = c;
            if (cfg.activeCount() > 0) {
                c.preconnect();   // connection is up by the time a command is chosen
            }
        }
    }

    function onStop(state as Dictionary?) as Void {
        var c = client;
        if (c != null) {
            c.close();
        }
    }

    function getInitialView() {
        return [new MainView(), new MainDelegate()];
    }

    // Settings were changed on the phone
    function onSettingsChanged() as Void {
        var cfg = new Config();
        var err = cfg.load();
        var old = config;
        var c = client;

        if (err != null) {
            configError = err;
        } else if (c == null) {
            // invalid before -> start for the first time now
            configError = null;
            config = cfg;
            c = new BleClient(cfg);
            c.setup();
            client = c;
        } else if (old != null && !old.uuidSignature.equals(cfg.uuidSignature)) {
            // the profile cannot be registered again at runtime
            restartNeeded = true;
        } else {
            configError = null;
            config = cfg;
            c.setConfig(cfg);
        }
        WatchUi.requestUpdate();
    }

    // true if commands can be sent
    function isReady() as Boolean {
        var cfg = config;
        return configError == null && !restartNeeded && client != null
            && cfg != null && cfg.activeCount() > 0;
    }

    // Text for the widget view
    function statusText() as String {
        if (restartNeeded) {
            return str(Rez.Strings.RestartHint1) + "\n" + str(Rez.Strings.RestartHint2);
        }
        if (configError != null) {
            return str(Rez.Strings.ConfigError) + "\n" + str(configError);
        }
        var cfg = config;
        if (cfg == null || cfg.activeCount() == 0) {
            return str(Rez.Strings.NoCommands);
        }
        return str(Rez.Strings.Hint1) + "\n" + str(Rez.Strings.Hint2);
    }
}

function getApp() as BleRemoteApp {
    return Application.getApp() as BleRemoteApp;
}

function str(id) as String {
    return WatchUi.loadResource(id) as String;
}
