package com.hexadecinull.qrscan.decode;

import com.google.zxing.BinaryBitmap;
import com.google.zxing.DecodeHintType;
import com.google.zxing.MultiFormatReader;
import com.google.zxing.RGBLuminanceSource;
import com.google.zxing.Result;
import com.google.zxing.common.HybridBinarizer;
import javax.microedition.lcdui.Image;
import java.util.Hashtable;

public final class BlackBerryDecoder {

    private BlackBerryDecoder() {}

    public static String decodeJpegBytes(byte[] jpegData) {
        if (jpegData == null || jpegData.length == 0) return null;
        try {
            net.rim.device.api.system.EncodedImage encoded =
                net.rim.device.api.system.EncodedImage.createEncodedImage(
                    jpegData, 0, jpegData.length);
            net.rim.device.api.ui.component.BitmapField bitmapField =
                new net.rim.device.api.ui.component.BitmapField(encoded.getBitmap());
            net.rim.device.api.system.Bitmap bitmap = encoded.getBitmap();
            int w = bitmap.getWidth();
            int h = bitmap.getHeight();
            int[] pixels = new int[w * h];
            bitmap.getARGB(pixels, 0, w, 0, 0, w, h);

            RGBLuminanceSource source = new RGBLuminanceSource(w, h, pixels);
            BinaryBitmap bmp = new BinaryBitmap(new HybridBinarizer(source));

            Hashtable hints = new Hashtable();
            hints.put(DecodeHintType.TRY_HARDER, Boolean.TRUE);
            hints.put(DecodeHintType.CHARACTER_SET, "UTF-8");

            MultiFormatReader reader = new MultiFormatReader();
            reader.setHints(hints);
            Result result = reader.decode(bmp);
            return result.getText();
        } catch (Exception e) {
            return null;
        }
    }
}
