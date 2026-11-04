// Progress display while sending and result display
import Toybox.Attention;
import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;

class SendController extends BleListener {
    private var mIndex as Number;
    private var mBar as WatchUi.ProgressBar;
    private var mView as ResultView? = null;  // set as soon as OK is shown

    function initialize(index as Number) {
        BleListener.initialize();
        mIndex = index;
        mBar = new WatchUi.ProgressBar(str(Rez.Strings.PhaseScan), null);
    }

    function start() as Void {
        WatchUi.pushView(mBar, new ProgressDelegate(self), WatchUi.SLIDE_IMMEDIATE);
        var c = getApp().client;
        if (c != null) {
            c.send(mIndex, self);
        } else {
            onBleResult(RES_BLE_ERROR);  // detail stays empty: invalid configuration
        }
    }

    function cancel() as Void {
        var c = getApp().client;
        if (c != null) {
            c.cancel();
        }
    }

    function onBleProgress(phase as Number) as Void {
        var id = Rez.Strings.PhaseScan;
        if (phase == PHASE_CONNECT) {
            id = Rez.Strings.PhaseConnect;
        } else if (phase == PHASE_SEND) {
            id = Rez.Strings.PhaseSend;
        }
        mBar.setDisplayString(str(id));
    }

    // ESP32 acknowledged the command: show OK at once, status comes later
    function onBleSent() as Void {
        if (mView != null) {
            return;  // sent again after a nonce retry
        }
        signal(true);
        var v = new ResultView(RES_OK, detail(RES_OK));
        mView = v;
        WatchUi.switchToView(v, new ResultDelegate(), WatchUi.SLIDE_IMMEDIATE);
    }

    function onBleResult(result as Number) as Void {
        if (result == RES_CANCELLED) {
            return;
        }
        var v = mView;
        if (v != null) {
            // OK is already shown: only switch on an error and vibrate long
            if (result != RES_OK) {
                signal(false);
            }
            v.update(result, detail(result));
            WatchUi.requestUpdate();
            return;
        }
        signal(result == RES_OK);
        WatchUi.switchToView(new ResultView(result, detail(result)), new ResultDelegate(),
                             WatchUi.SLIDE_IMMEDIATE);
    }

    private function detail(result as Number) as String {
        var c = getApp().client;
        return ((result == RES_BLE_ERROR || result == RES_NOT_FOUND || result == RES_OK)
                && c != null) ? c.errDetail : "";
    }

    private function signal(ok as Boolean) as Void {
        if (Attention has :vibrate) {
            Attention.vibrate([new Attention.VibeProfile(60, ok ? 200 : 700)]);
        }
    }
}

class ProgressDelegate extends WatchUi.BehaviorDelegate {
    private var mCtrl as SendController;

    function initialize(ctrl as SendController) {
        BehaviorDelegate.initialize();
        mCtrl = ctrl;
    }

    function onBack() as Boolean {
        mCtrl.cancel();
        WatchUi.popView(WatchUi.SLIDE_IMMEDIATE);
        return true;
    }
}

class ResultView extends WatchUi.View {
    private var mResult as Number;
    private var mDetail as String;

    function initialize(result as Number, detail as String) {
        View.initialize();
        mResult = result;
        mDetail = detail;
    }

    function update(result as Number, detail as String) as Void {
        mResult = result;
        mDetail = detail;
    }

    function onUpdate(dc as Graphics.Dc) as Void {
        var ok = (mResult == RES_OK);
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_BLACK);
        dc.clear();
        dc.setColor(ok ? Graphics.COLOR_GREEN : Graphics.COLOR_RED,
                    Graphics.COLOR_TRANSPARENT);
        dc.drawText(dc.getWidth() / 2, dc.getHeight() / 2,
                    (ok && mDetail.length() == 0) ? Graphics.FONT_MEDIUM : Graphics.FONT_TINY,
                    mDetail.length() > 0 ? str(resultText(mResult)) + "\n" + mDetail
                                         : str(resultText(mResult)),
                    Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }
}

class ResultDelegate extends WatchUi.BehaviorDelegate {
    function initialize() {
        BehaviorDelegate.initialize();
    }

    // Back or START -> back to the menu
    function onBack() as Boolean {
        WatchUi.popView(WatchUi.SLIDE_IMMEDIATE);
        return true;
    }

    function onSelect() as Boolean {
        WatchUi.popView(WatchUi.SLIDE_IMMEDIATE);
        return true;
    }
}

function resultText(result as Number) {
    switch (result) {
        case RES_OK:             return Rez.Strings.ResOk;
        case RES_NOT_FOUND:      return Rez.Strings.ResNotFound;
        case RES_CONNECT_FAILED: return Rez.Strings.ResConnect;
        case RES_BAD_KEY:        return Rez.Strings.ResBadKey;
        case RES_UNKNOWN_CMD:    return Rez.Strings.ResUnknownCmd;
        case RES_BAD_FORMAT:     return Rez.Strings.ResBadFormat;
        case RES_EXEC_FAILED:    return Rez.Strings.ResExec;
        case RES_TIMEOUT:        return Rez.Strings.ResTimeout;
    }
    return Rez.Strings.ResBle;
}
