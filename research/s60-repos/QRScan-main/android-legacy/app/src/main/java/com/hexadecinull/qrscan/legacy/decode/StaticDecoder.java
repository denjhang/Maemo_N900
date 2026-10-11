package com.hexadecinull.qrscan.legacy.decode;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import com.google.zxing.BinaryBitmap;
import com.google.zxing.DecodeHintType;
import com.google.zxing.MultiFormatReader;
import com.google.zxing.RGBLuminanceSource;
import com.google.zxing.Result;
import com.google.zxing.common.HybridBinarizer;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;

public final class StaticDecoder {

    private StaticDecoder() {}

    public static DecodeResult decode(Context context, Uri uri) {
        Bitmap bmp = loadBitmap(context, uri);
        if (bmp == null) return null;
        return decodeBitmap(bmp);
    }

    public static DecodeResult decodeBitmap(Bitmap bmp) {
        int w = bmp.getWidth();
        int h = bmp.getHeight();
        int[] pixels = new int[w * h];
        bmp.getPixels(pixels, 0, w, 0, 0, w, h);
        RGBLuminanceSource source = new RGBLuminanceSource(w, h, pixels);
        BinaryBitmap binary = new BinaryBitmap(new HybridBinarizer(source));
        Map<DecodeHintType, Object> hints = new HashMap<DecodeHintType, Object>();
        hints.put(DecodeHintType.TRY_HARDER, Boolean.TRUE);
        hints.put(DecodeHintType.CHARACTER_SET, "UTF-8");
        MultiFormatReader reader = new MultiFormatReader();
        reader.setHints(hints);
        try {
            Result result = reader.decode(binary);
            return new DecodeResult(result.getText(), result.getBarcodeFormat().name());
        } catch (Exception e) {
            return null;
        }
    }

    private static Bitmap loadBitmap(Context context, Uri uri) {
        try {
            InputStream in = context.getContentResolver().openInputStream(uri);
            if (in == null) return null;
            BitmapFactory.Options opts = new BitmapFactory.Options();
            opts.inSampleSize = 1;
            Bitmap raw = BitmapFactory.decodeStream(in, null, opts);
            in.close();
            if (raw == null) return null;
            int pixels = raw.getWidth() * raw.getHeight();
            if (pixels > 1500000) {
                float scale = (float) Math.sqrt(1500000.0 / pixels);
                int nw = (int) (raw.getWidth() * scale);
                int nh = (int) (raw.getHeight() * scale);
                return Bitmap.createScaledBitmap(raw, nw, nh, true);
            }
            return raw;
        } catch (Exception e) {
            return null;
        }
    }
}
