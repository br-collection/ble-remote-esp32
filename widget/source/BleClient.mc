// BLE sequence: scan -> connect -> read nonce -> write command ->
// read status (repeated while "busy")
// The connection is set up as soon as the widget opens and then stays open
// for KEEP_MS, so a command does not have to scan and connect every time.
// The firmware disconnects 20 s after connecting (SESSION_TIMEOUT_MS in
// firmware/include/config.h); the widget reconnects before that (LINK_MAX_MS).
import Toybox.BluetoothLowEnergy;
import Toybox.Lang;
import Toybox.System;
import Toybox.Timer;

class BleClient extends BluetoothLowEnergy.BleDelegate {

    enum {
        ST_IDLE,
        ST_SCANNING,
        ST_CONNECTING,
        ST_READY,        // connected, no command
        ST_PREFETCH,     // connected, reading the nonce in advance
        ST_READ_NONCE,
        ST_WRITING,
        ST_READ_STATUS,
        ST_WAIT_POLL
    }

    const CONNECT_TIMEOUT_MS = 15000;  // from device found or command to result
    const POLL_MS = 50;
    const KEEP_MS = 60000;             // keep the connection this long after opening or the last command
    const LINK_MAX_MS = 14000;         // then reconnect (firmware disconnects after 20 s)
    const MAX_RECONNECTS = 2;          // connection lost during setup: scan again this many times
    const NONCE_MAX_AGE_MS = 20000;    // use a nonce read in advance this long (firmware: valid for 30 s)

    private var mConfig as Config;
    private var mListener as BleListener? = null;
    private var mState as Number = ST_IDLE;
    private var mProfileReady as Boolean = false;
    private var mProfileFailed as Boolean = false;
    private var mProfileStatus as Number = -1;   // -1 = exception in registerProfile

    private var mCmd as Number = 0;              // 0 = no command active
    private var mPayload as ByteArray = []b;
    private var mNonce as ByteArray? = null;     // unused nonce of the current connection
    private var mNonceTime as Number = 0;
    private var mRetried as Boolean = false;

    private var mDevice = null;      // BluetoothLowEnergy.Device
    private var mNonceChr = null;    // BluetoothLowEnergy.Characteristic
    private var mCmdChr = null;
    private var mStatusChr = null;

    private var mKeepUntil as Number = 0;   // System.getTimer(), keep the connection until then
    private var mLinkTime as Number = 0;    // time of connecting
    private var mReconnects as Number = 0;

    private var mTimeout as Timer.Timer;
    // location of the last BLE error, shown with the result
    var errDetail as String = "";
    // counters of the last scan (for "not found")
    private var mSeen as Number = 0;     // scan results
    private var mNamed as Number = 0;    // of those with a name
    private var mUuidHit as Number = 0;  // of those with a matching service UUID
    private var mLastName as String = "";
    private var mPoll as Timer.Timer;
    // times (System.getTimer) for the timing display on OK
    private var mT0 as Number = 0;
    private var mTFound as Number = 0;
    private var mTConn as Number = 0;
    private var mTWritten as Number = 0;  // write acknowledged
    private var mReads as Number = 0;     // status reads
    private var mPre as Boolean = false;  // command over an existing connection

    function initialize(config as Config) {
        BleDelegate.initialize();
        mConfig = config;
        mTimeout = new Timer.Timer();
        mPoll = new Timer.Timer();
    }

    // Set the delegate and register the GATT profile (asynchronous)
    function setup() as Void {
        BluetoothLowEnergy.setDelegate(self);
        try {
            BluetoothLowEnergy.registerProfile({
                :uuid => mConfig.uuidService,
                :characteristics => [
                    { :uuid => mConfig.uuidNonce },
                    { :uuid => mConfig.uuidCommand },
                    { :uuid => mConfig.uuidStatus }
                ]
            });
        } catch (e) {
            mProfileFailed = true;
        }
    }

    // Take over a new configuration with the same UUIDs
    function setConfig(config as Config) as Void {
        mConfig = config;
    }

    // Set up the connection in advance and keep it for KEEP_MS
    function preconnect() as Void {
        mKeepUntil = System.getTimer() + KEEP_MS;
        if (mProfileFailed) {
            return;
        }
        if (mState == ST_IDLE) {
            mReconnects = 0;
            startScan();
        } else if (mState == ST_READY) {
            enterReady();   // keep time extended
        }
    }

    // Send command 1-4
    function send(index as Number, listener as BleListener) as Void {
        if (mCmd > 0) {
            // Previous command is still reading the status (OK already shown): cancel it
            mListener = null;
            finishCmd(RES_CANCELLED);
        }
        mListener = listener;
        mCmd = index;
        mPayload = mConfig.payloads[index - 1];
        mRetried = false;

        errDetail = "";
        mT0 = System.getTimer();
        mTFound = 0;
        mTConn = 0;
        mTWritten = 0;
        mReads = 0;
        mPre = false;
        mKeepUntil = mT0 + KEEP_MS;
        if (mProfileFailed) {
            fail(mProfileStatus < 0 ? "P1 profile" : "P2 profile " + mProfileStatus);
            return;
        }
        if (mState == ST_READY && mT0 - mLinkTime > LINK_MAX_MS) {
            closeLink();    // firmware will disconnect soon: better reconnect now
        }
        // Do not wait for onProfileRegister: it does not arrive on some watches
        // (FR945). The profile is only needed when connecting.
        if (mState == ST_READY || mState == ST_PREFETCH) {
            mPre = true;
            mTFound = mT0;
            mTConn = mT0;
            if (mState == ST_READY) {
                startCommand();
            } else {
                // the nonce is about to arrive, then the write follows
                progress(PHASE_SEND);
                restartTimeout(CONNECT_TIMEOUT_MS);
            }
        } else if (mState == ST_CONNECTING) {
            // setup already running (started on opening)
            mTFound = mT0;
            restartTimeout(CONNECT_TIMEOUT_MS);
            progress(PHASE_CONNECT);
        } else if (mState == ST_SCANNING) {
            restartTimeout(mConfig.scanTimeoutMs);
            progress(PHASE_SCAN);
        } else {
            mReconnects = 0;
            progress(PHASE_SCAN);
            startScan();
        }
    }

    // Cancelled by the user, no feedback. A connection setup in progress
    // continues in the background.
    function cancel() as Void {
        mListener = null;
        if (mCmd > 0) {
            finishCmd(RES_CANCELLED);
        }
    }

    // Widget is closing: disconnect everything
    function close() as Void {
        mListener = null;
        mCmd = 0;
        mKeepUntil = System.getTimer();
        closeLink();
    }

    // ----- Sequence --------------------------------------------------------

    private function startScan() as Void {
        mSeen = 0;
        mNamed = 0;
        mUuidHit = 0;
        mLastName = "";
        mState = ST_SCANNING;
        restartTimeout(mConfig.scanTimeoutMs);
        BluetoothLowEnergy.setScanState(BluetoothLowEnergy.SCAN_STATE_SCANNING);
    }

    // A matching service UUID is required. The ESP32 only has the name in
    // the scan response and it can be missing: the filter only rejects other names.
    private function matches(r) as Boolean {
        mSeen++;
        var name = r.getDeviceName();
        if (name != null && name.length() > 0) {
            mNamed++;
            mLastName = name;
        }
        var hit = false;
        var uuids = r.getServiceUuids();
        for (var u = uuids.next(); u != null; u = uuids.next()) {
            if (mConfig.uuidService.equals(u)) {
                hit = true;
            }
        }
        if (!hit) {
            return false;
        }
        mUuidHit++;
        if (mConfig.nameFilter.length() > 0 && name != null && name.length() > 0
                && !mConfig.nameFilter.equals(name)) {
            return false;
        }
        return true;
    }

    private function connect(r) as Void {
        BluetoothLowEnergy.setScanState(BluetoothLowEnergy.SCAN_STATE_OFF);
        mState = ST_CONNECTING;
        mTFound = System.getTimer();
        restartTimeout(CONNECT_TIMEOUT_MS);
        progress(PHASE_CONNECT);
        try {
            mDevice = BluetoothLowEnergy.pairDevice(r);
        } catch (e) {
            mDevice = null;
        }
        if (mDevice == null) {
            linkLost();
        }
    }

    private function startCommand() as Void {
        progress(PHASE_SEND);
        restartTimeout(CONNECT_TIMEOUT_MS);
        if (mConfig.key != null && !nonceFresh()) {
            readNonce();
        } else {
            writeFrame();
        }
    }

    private function nonceFresh() as Boolean {
        return mNonce != null && System.getTimer() - mNonceTime < NONCE_MAX_AGE_MS;
    }

    // Connected without a command: read the nonce now, so a command
    // can be written at once
    private function prefetchNonce() as Void {
        if (mConfig.key == null || nonceFresh()) {
            enterReady();
            return;
        }
        mState = ST_PREFETCH;
        restartTimeout(CONNECT_TIMEOUT_MS);
        try {
            mNonceChr.requestRead();
        } catch (e) {
            fail("R1 nonce");
        }
    }

    private function readNonce() as Void {
        mState = ST_READ_NONCE;
        try {
            mNonceChr.requestRead();
        } catch (e) {
            fail("R1 nonce");
        }
    }

    private function writeFrame() as Void {
        var frame = Protocol.buildFrame(mCmd, mPayload, mConfig.key, mNonce);
        mNonce = null;   // the firmware accepts each nonce for one attempt only
        mState = ST_WRITING;
        try {
            mCmdChr.requestWrite(frame,
                { :writeType => BluetoothLowEnergy.WRITE_TYPE_WITH_RESPONSE });
        } catch (e) {
            fail("W1 write");
        }
    }

    private function readStatus() as Void {
        mState = ST_READ_STATUS;
        mReads++;
        try {
            mStatusChr.requestRead();
        } catch (e) {
            fail("R2 status");
        }
    }

    private function handleStatus(code as Number) as Void {
        if (code == Protocol.ST_OK) {
            finishCmd(RES_OK);
        } else if (code == Protocol.ST_BUSY || code == Protocol.ST_NO_COMMAND) {
            mState = ST_WAIT_POLL;
            mPoll.stop();
            mPoll.start(method(:onPollTimer), POLL_MS, false);
        } else if (code == Protocol.ST_NONCE_INVALID) {
            if (!mRetried && mConfig.key != null) {
                mRetried = true;      // retry once with a new nonce
                readNonce();
            } else {
                finishCmd(RES_TIMEOUT);
            }
        } else if (code == Protocol.ST_BAD_MAC) {
            finishCmd(RES_BAD_KEY);
        } else if (code == Protocol.ST_UNKNOWN_CMD) {
            finishCmd(RES_UNKNOWN_CMD);
        } else if (code == Protocol.ST_BAD_FORMAT) {
            finishCmd(RES_BAD_FORMAT);
        } else if (code == Protocol.ST_EXEC_FAILED) {
            finishCmd(RES_EXEC_FAILED);
        } else {
            fail("X status " + code.format("%02X"));
        }
    }

    private function fail(detail as String) as Void {
        errDetail = detail;
        if (mCmd > 0) {
            finishCmd(RES_BLE_ERROR);
        } else {
            closeLink();
        }
    }

    // Connection lost during setup or before writing: scan again,
    // as long as a command is waiting or the keep time is running
    private function linkLost() as Void {
        closeLink();
        if (mReconnects < MAX_RECONNECTS && (mCmd > 0 || mKeepUntil - System.getTimer() > 0)) {
            mReconnects++;
            progress(PHASE_SCAN);
            startScan();
        } else if (mCmd > 0) {
            finishCmd(RES_CONNECT_FAILED);
        }
    }

    // Finish the command and report the result. The connection stays if
    // it is up and the error is not caused by it.
    private function finishCmd(result as Number) as Void {
        mPoll.stop();
        mCmd = 0;

        if (result == RES_OK && mTConn > 0) {
            // scan, connect, send in seconds
            var now = System.getTimer();
            errDetail = (mPre ? "linked" : "Sc " + secs(mTFound - mT0) + "  Co " + secs(mTConn - mTFound))
                + "  Se " + secs(now - mTConn);
            // send split up: until the write is acknowledged, then status reads (count)
            if (mTWritten > 0) {
                errDetail += "\nWr " + secs(mTWritten - mTConn)
                    + "  Rd " + secs(now - mTWritten) + " x" + mReads;
            }
        }

        var l = mListener;
        mListener = null;

        // Cancelled during the scan: setup continues in the background
        var background = result == RES_CANCELLED
            && (mState == ST_SCANNING || mState == ST_CONNECTING);
        var keep = mCmdChr != null && (result == RES_OK || result == RES_CANCELLED
            || result == RES_BAD_KEY || result == RES_UNKNOWN_CMD
            || result == RES_BAD_FORMAT || result == RES_EXEC_FAILED);
        var dev = null;
        if (!background) {
            mTimeout.stop();
            if (keep) {
                prefetchNonce();   // the firmware has a new nonce after the command
            } else {
                dev = dropLink();
            }
        }

        // Report the result first, disconnect afterwards (can take a while)
        if (l != null) {
            l.onBleResult(result);
        }
        unpair(dev);
    }

    // Connected without a command: wait until the keep time ends, but
    // reconnect before the firmware disconnects
    private function enterReady() as Void {
        mState = ST_READY;
        var now = System.getTimer();
        var ms = mKeepUntil - now;
        var renew = mLinkTime + LINK_MAX_MS - now;
        if (renew < ms) {
            ms = renew;
        }
        if (ms < 50) {
            ms = 50;
        }
        restartTimeout(ms);
    }

    // Reset the state, returns the device to unpair
    private function dropLink() {
        if (mState == ST_SCANNING) {
            BluetoothLowEnergy.setScanState(BluetoothLowEnergy.SCAN_STATE_OFF);
        }
        mState = ST_IDLE;   // callbacks are ignored from here on
        mNonce = null;      // only valid for this connection
        var dev = mDevice;
        mDevice = null;
        mNonceChr = null;
        mCmdChr = null;
        mStatusChr = null;
        return dev;
    }

    private function unpair(dev) as Void {
        if (dev != null) {
            try {
                BluetoothLowEnergy.unpairDevice(dev);
            } catch (e) {
            }
        }
    }

    private function closeLink() as Void {
        mTimeout.stop();
        mPoll.stop();
        unpair(dropLink());
    }

    private function secs(ms as Number) as String {
        return (ms / 1000.0).format("%.1f");
    }

    private function progress(phase as Number) as Void {
        var l = mListener;
        if (l != null) {
            l.onBleProgress(phase);
        }
    }

    private function restartTimeout(ms as Number) as Void {
        mTimeout.stop();
        mTimeout.start(method(:onTimeout), ms, false);
    }

    // ----- Timers ---------------------------------------------------------

    function onTimeout() as Void {
        if (mState == ST_SCANNING) {
            if (mCmd == 0) {
                closeLink();   // background scan without success
                return;
            }
            // D = devices seen, N = with a name, U = matching UUID, P = profile
            errDetail = "D" + mSeen + " N" + mNamed + " U" + mUuidHit
                + " P" + (mProfileReady ? "1" : (mProfileFailed ? "-" : "?"));
            if (mLastName.length() > 0) {
                errDetail += "\n" + mLastName;
            }
            finishCmd(RES_NOT_FOUND);
        } else if (mState == ST_READY) {
            closeLink();
            if (mKeepUntil - System.getTimer() > 0) {
                mReconnects = 0;
                startScan();   // keep time still running: reconnect
            }
        } else if (mState != ST_IDLE) {
            if (mCmd > 0) {
                finishCmd(RES_TIMEOUT);
            } else {
                closeLink();
            }
        }
    }

    function onPollTimer() as Void {
        if (mState == ST_WAIT_POLL) {
            readStatus();
        }
    }

    // ----- BLE callbacks -------------------------------------------------

    function onProfileRegister(uuid, status) {
        if (status == BluetoothLowEnergy.STATUS_SUCCESS) {
            mProfileReady = true;
        } else {
            mProfileFailed = true;
            mProfileStatus = status;
        }
    }

    function onScanResults(scanResults) {
        if (mState != ST_SCANNING) {
            return;
        }
        for (var r = scanResults.next(); r != null; r = scanResults.next()) {
            if (matches(r)) {
                connect(r);
                return;
            }
        }
    }

    function onConnectedStateChanged(device, state) {
        if (state == BluetoothLowEnergy.CONNECTION_STATE_CONNECTED) {
            if (mState != ST_CONNECTING) {
                return;
            }
            mDevice = device;
            mTConn = System.getTimer();
            mLinkTime = mTConn;
            mReconnects = 0;
            var svc = device.getService(mConfig.uuidService);
            if (svc == null) {
                fail(mProfileReady ? "S service missing" : "S service, profile ?");
                return;
            }
            mNonceChr = svc.getCharacteristic(mConfig.uuidNonce);
            mCmdChr = svc.getCharacteristic(mConfig.uuidCommand);
            mStatusChr = svc.getCharacteristic(mConfig.uuidStatus);
            if (mCmdChr == null || mStatusChr == null
                    || (mConfig.key != null && mNonceChr == null)) {
                fail("C characteristic missing");
                return;
            }
            if (mCmd > 0) {
                startCommand();
            } else {
                prefetchNonce();
            }
        } else if (mState == ST_CONNECTING || mState == ST_READY || mState == ST_PREFETCH
                || mState == ST_READ_NONCE) {
            // Nothing written yet: reconnecting is safe
            linkLost();
        } else if (mState == ST_READ_STATUS || mState == ST_WAIT_POLL) {
            // The command arrived (write acknowledged), only the status is missing
            errDetail += "\nStatus ?";
            mTConn = 0;      // do not overwrite the timing display
            mCmdChr = null;  // connection is gone, do not keep it
            finishCmd(RES_OK);
            if (mState == ST_IDLE && mKeepUntil - System.getTimer() > 0) {
                startScan();
            }
        } else if (mState != ST_IDLE && mState != ST_SCANNING) {
            finishCmd(RES_CONNECT_FAILED);   // lost while writing: do not repeat
        }
    }

    function onCharacteristicRead(characteristic, status, value) {
        var nonce = (mState == ST_READ_NONCE || mState == ST_PREFETCH);
        if (!nonce && mState != ST_READ_STATUS) {
            return;
        }
        if (status != BluetoothLowEnergy.STATUS_SUCCESS || value == null) {
            fail((nonce ? "R3 nonce " : "R4 status ") + status);
            return;
        }
        if (nonce) {
            if (value.size() != Protocol.NONCE_LEN) {
                fail("N nonce " + value.size());
                return;
            }
            mNonce = value;
            mNonceTime = System.getTimer();
            if (mCmd > 0) {
                writeFrame();
            } else {
                enterReady();
            }
        } else {
            if (value.size() < 1) {
                fail("R5 status empty");
                return;
            }
            handleStatus(value[0]);
        }
    }

    function onCharacteristicWrite(characteristic, status) {
        if (mState != ST_WRITING) {
            return;
        }
        if (status != BluetoothLowEnergy.STATUS_SUCCESS) {
            fail("W2 write " + status);
        } else {
            mTWritten = System.getTimer();
            // ESP32 has the command: report at once, read the status afterwards
            // (FR945: 1 s connection interval, a read takes about 5 s)
            errDetail = (mPre ? "linked" : "Sc " + secs(mTFound - mT0) + "  Co " + secs(mTConn - mTFound))
                + "  Wr " + secs(mTWritten - mTConn);
            var l = mListener;
            if (l != null) {
                l.onBleSent();
            }
            readStatus();
        }
    }
}
