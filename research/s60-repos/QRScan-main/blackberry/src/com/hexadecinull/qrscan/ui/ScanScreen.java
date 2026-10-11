package com.hexadecinull.qrscan.ui;

import net.rim.device.api.ui.Screen;
import net.rim.device.api.ui.component.LabelField;
import net.rim.device.api.ui.component.ButtonField;
import net.rim.device.api.ui.container.MainScreen;
import net.rim.device.api.ui.UiApplication;
import net.rim.device.api.ui.MenuItem;
import net.rim.device.api.ui.component.Dialog;
import net.rim.device.api.system.Application;

import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.media.control.VideoControl;
import javax.microedition.media.control.FocusControl;
import javax.microedition.media.control.FlashControl;

import com.hexadecinull.qrscan.decode.BlackBerryDecoder;
import com.hexadecinull.qrscan.util.HistoryManager;

public final class ScanScreen extends MainScreen {

    private Player player;
    private VideoControl videoControl;
    private FlashControl flashControl;
    private boolean flashOn = false;
    private java.awt.Container videoDisplay;

    public ScanScreen() {
        super(DEFAULT_CLOSE);
        setTitle("QRScan");
        buildMenu();
        initCamera();
    }

    private void initCamera() {
        try {
            player = Manager.createPlayer("capture://video?encoding=jpeg");
            player.realize();

            videoControl = (VideoControl) player.getControl("VideoControl");
            if (videoControl != null) {
                videoDisplay = (java.awt.Container) videoControl
                    .initDisplayMode(VideoControl.USE_GUI_PRIMITIVE,
                        "net.rim.device.api.ui.Field");
                add((net.rim.device.api.ui.Field) videoDisplay);
            }

            flashControl = (FlashControl) player.getControl("FlashControl");

            LabelField hint = new LabelField("Point camera at QR code",
                LabelField.ELLIPSIS | LabelField.USE_ALL_WIDTH);
            add(hint);

            ButtonField flashBtn = new ButtonField("Toggle Flash",
                ButtonField.CONSUME_CLICK);
            flashBtn.setChangeListener(new net.rim.device.api.ui.FieldChangeListener() {
                public void fieldChanged(net.rim.device.api.ui.Field field, int context) {
                    toggleFlash();
                }
            });
            add(flashBtn);

            ButtonField pickBtn = new ButtonField("Pick Image",
                ButtonField.CONSUME_CLICK);
            pickBtn.setChangeListener(new net.rim.device.api.ui.FieldChangeListener() {
                public void fieldChanged(net.rim.device.api.ui.Field field, int context) {
                    pickImage();
                }
            });
            add(pickBtn);

            player.start();
            startDecodeLoop();

        } catch (Exception e) {
            add(new LabelField("Camera unavailable. Use 'Pick Image' to scan."));
        }
    }

    private void startDecodeLoop() {
        Thread t = new Thread(new Runnable() {
            public void run() {
                while (true) {
                    try {
                        Thread.sleep(800);
                        if (videoControl == null) break;
                        byte[] snap = (byte[]) videoControl.getSnapshot("encoding=jpeg&width=640&height=480");
                        final String result = BlackBerryDecoder.decodeJpegBytes(snap);
                        if (result != null) {
                            UiApplication.getUiApplication().invokeLater(new Runnable() {
                                public void run() {
                                    HistoryManager.getInstance().add(result, "QR_CODE");
                                    UiApplication.getUiApplication().pushScreen(
                                        new ResultScreen(result, "QR_CODE"));
                                }
                            });
                            break;
                        }
                    } catch (Exception ignored) {
                    }
                }
            }
        });
        t.setDaemon(true);
        t.start();
    }

    private void toggleFlash() {
        if (flashControl == null) return;
        try {
            flashOn = !flashOn;
            flashControl.setFlash(flashOn ? FlashControl.TORCH : FlashControl.OFF);
        } catch (Exception ignored) {
        }
    }

    private void pickImage() {
        try {
            net.rim.device.api.io.file.FileDialog fd =
                new net.rim.device.api.io.file.FileDialog(
                    net.rim.device.api.io.file.FileDialog.OPEN);
            fd.setFilter(new String[]{".jpg", ".jpeg", ".png", ".bmp"});
            if (fd.doModal() == net.rim.device.api.ui.component.Dialog.OK) {
                String path = fd.getSelectedPath();
                byte[] data = readFile(path);
                if (data != null) {
                    String result = BlackBerryDecoder.decodeJpegBytes(data);
                    if (result != null) {
                        HistoryManager.getInstance().add(result, "QR_CODE");
                        UiApplication.getUiApplication().pushScreen(
                            new ResultScreen(result, "QR_CODE"));
                    } else {
                        Dialog.alert("No QR code found in image.");
                    }
                }
            }
        } catch (Exception e) {
            Dialog.alert("Pick image failed: " + e.getMessage());
        }
    }

    private byte[] readFile(String path) {
        try {
            javax.microedition.io.Connection conn =
                javax.microedition.io.Connector.open(path);
            if (conn instanceof javax.microedition.io.InputConnection) {
                java.io.InputStream is =
                    ((javax.microedition.io.InputConnection) conn).openInputStream();
                java.io.ByteArrayOutputStream baos = new java.io.ByteArrayOutputStream();
                byte[] buf = new byte[4096];
                int len;
                while ((len = is.read(buf)) != -1) baos.write(buf, 0, len);
                is.close();
                conn.close();
                return baos.toByteArray();
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    private void buildMenu() {
        MenuItem historyItem = new MenuItem("History", 100, 1) {
            public void run() {
                UiApplication.getUiApplication().pushScreen(new HistoryScreen());
            }
        };
        MenuItem createItem = new MenuItem("Create QR", 200, 2) {
            public void run() {
                UiApplication.getUiApplication().pushScreen(new CreateScreen());
            }
        };
        addMenuItem(historyItem);
        addMenuItem(createItem);
    }

    protected void onUiEngineAttached(boolean attached) {
        super.onUiEngineAttached(attached);
        if (!attached && player != null) {
            try { player.stop(); player.close(); } catch (Exception ignored) {}
        }
    }
}
