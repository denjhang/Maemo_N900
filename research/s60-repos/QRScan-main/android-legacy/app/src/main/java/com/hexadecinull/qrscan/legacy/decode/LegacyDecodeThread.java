package com.hexadecinull.qrscan.legacy.decode;

import android.hardware.Camera;
import android.os.Handler;
import android.os.Looper;
import com.google.zxing.BinaryBitmap;
import com.google.zxing.DecodeHintType;
import com.google.zxing.MultiFormatReader;
import com.google.zxing.PlanarYUVLuminanceSource;
import com.google.zxing.Result;
import com.google.zxing.common.HybridBinarizer;
import com.hexadecinull.qrscan.legacy.camera.CameraManager;
import com.hexadecinull.qrscan.legacy.camera.PreviewCallback;
import java.util.HashMap;
import java.util.Map;

@SuppressWarnings("deprecation")
public final class LegacyDecodeThread extends Thread implements Camera.PreviewCallback {

    private final CameraManager cameraManager;
    private final PreviewCallback.Listener listener;
    private final MultiFormatReader reader;
    private volatile boolean running = true;
    private Looper looper;
    private final Object lock = new Object();
    private byte[] pendingData;
    private int pendingWidth;
    private int pendingHeight;

    public LegacyDecodeThread(CameraManager cameraManager, PreviewCallback.Listener listener) {
        this.cameraManager = cameraManager;
        this.listener = listener;
        Map<DecodeHintType, Object> hints = new HashMap<DecodeHintType, Object>();
        hints.put(DecodeHintType.TRY_HARDER, Boolean.TRUE);
        hints.put(DecodeHintType.CHARACTER_SET, "UTF-8");
        reader = new MultiFormatReader();
        reader.setHints(hints);
    }

    @Override
    public void run() {
        Looper.prepare();
        looper = Looper.myLooper();
        requestNextFrame();
        Looper.loop();
    }

    public void quit() {
        running = false;
        if (looper != null) looper.quit();
    }

    private void requestNextFrame() {
        if (!running) return;
        cameraManager.requestPreviewFrame(this);
    }

    @Override
    public void onPreviewFrame(final byte[] data, Camera camera) {
        if (!running) return;
        Camera.Size size = cameraManager.getPreviewSize();
        if (size == null) {
            requestNextFrame();
            return;
        }
        Result result = decode(data, size.width, size.height);
        if (result != null) {
            listener.onDecodeResult(new DecodeResult(result.getText(), result.getBarcodeFormat().name()));
        } else {
            requestNextFrame();
        }
    }

    private Result decode(byte[] data, int width, int height) {
        PlanarYUVLuminanceSource source = new PlanarYUVLuminanceSource(
            data, width, height, 0, 0, width, height, false);
        BinaryBitmap bmp = new BinaryBitmap(new HybridBinarizer(source));
        try {
            return reader.decodeWithState(bmp);
        } catch (Exception e) {
            return null;
        } finally {
            reader.reset();
        }
    }
}
