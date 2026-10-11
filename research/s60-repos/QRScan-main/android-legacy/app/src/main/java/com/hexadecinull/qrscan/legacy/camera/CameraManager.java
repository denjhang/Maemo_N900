package com.hexadecinull.qrscan.legacy.camera;

import android.content.Context;
import android.hardware.Camera;
import android.view.SurfaceHolder;
import java.io.IOException;
import java.util.List;

@SuppressWarnings("deprecation")
public final class CameraManager {

    private final Context context;
    private Camera camera;
    private boolean previewing = false;

    public CameraManager(Context context) {
        this.context = context;
    }

    public boolean isOpen() {
        return camera != null;
    }

    public void openDriver(SurfaceHolder holder) throws IOException {
        if (camera == null) {
            try {
                camera = Camera.open();
            } catch (RuntimeException e) {
                camera = null;
                throw new IOException("Camera unavailable: " + e.getMessage());
            }
        }
        camera.setPreviewDisplay(holder);
        Camera.Parameters params = camera.getParameters();
        applyBestParameters(params);
        camera.setParameters(params);
    }

    private void applyBestParameters(Camera.Parameters params) {
        List<String> focusModes = params.getSupportedFocusModes();
        if (focusModes != null) {
            if (focusModes.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE)) {
                params.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
            } else if (focusModes.contains(Camera.Parameters.FOCUS_MODE_AUTO)) {
                params.setFocusMode(Camera.Parameters.FOCUS_MODE_AUTO);
            }
        }
        params.setPreviewFormat(android.graphics.ImageFormat.NV21);
        Camera.Size best = getBestPreviewSize(params);
        if (best != null) {
            params.setPreviewSize(best.width, best.height);
        }
    }

    private Camera.Size getBestPreviewSize(Camera.Parameters params) {
        List<Camera.Size> sizes = params.getSupportedPreviewSizes();
        if (sizes == null || sizes.isEmpty()) return null;
        Camera.Size best = sizes.get(0);
        for (Camera.Size size : sizes) {
            int area = size.width * size.height;
            int bestArea = best.width * best.height;
            if (area > bestArea && area <= 1280 * 720) {
                best = size;
            }
        }
        return best;
    }

    public void startPreview() {
        if (camera != null && !previewing) {
            camera.startPreview();
            previewing = true;
        }
    }

    public void stopPreview() {
        if (camera != null && previewing) {
            camera.stopPreview();
            previewing = false;
        }
    }

    public void closeDriver() {
        if (camera != null) {
            camera.release();
            camera = null;
        }
    }

    public void requestPreviewFrame(Camera.PreviewCallback callback) {
        if (camera != null && previewing) {
            camera.setOneShotPreviewCallback(callback);
        }
    }

    public void setTorch(boolean on) {
        if (camera == null) return;
        try {
            Camera.Parameters params = camera.getParameters();
            List<String> flashModes = params.getSupportedFlashModes();
            if (flashModes == null) return;
            String mode = on ? Camera.Parameters.FLASH_MODE_TORCH
                             : Camera.Parameters.FLASH_MODE_OFF;
            if (flashModes.contains(mode)) {
                params.setFlashMode(mode);
                camera.setParameters(params);
            }
        } catch (RuntimeException ignored) {
        }
    }

    public Camera.Size getPreviewSize() {
        if (camera == null) return null;
        return camera.getParameters().getPreviewSize();
    }
}
