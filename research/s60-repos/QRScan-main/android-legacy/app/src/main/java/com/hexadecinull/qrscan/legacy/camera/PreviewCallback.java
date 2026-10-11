package com.hexadecinull.qrscan.legacy.camera;

import com.hexadecinull.qrscan.legacy.decode.DecodeResult;

public final class PreviewCallback {
    public interface Listener {
        void onDecodeResult(DecodeResult result);
    }

    private PreviewCallback() {}
}
