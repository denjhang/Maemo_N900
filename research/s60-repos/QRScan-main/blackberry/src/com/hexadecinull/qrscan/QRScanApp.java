package com.hexadecinull.qrscan;

import net.rim.device.api.ui.UiApplication;

public final class QRScanApp extends UiApplication {

    public static void main(String[] args) {
        QRScanApp app = new QRScanApp();
        app.enterEventDispatcher();
    }

    private QRScanApp() {
        pushScreen(new com.hexadecinull.qrscan.ui.ScanScreen());
    }
}
