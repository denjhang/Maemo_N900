package com.hexadecinull.qrscan.ui;

import net.rim.device.api.ui.container.MainScreen;
import net.rim.device.api.ui.component.BasicEditField;
import net.rim.device.api.ui.component.ButtonField;
import net.rim.device.api.ui.component.BitmapField;
import net.rim.device.api.ui.component.LabelField;
import net.rim.device.api.ui.component.Dialog;
import net.rim.device.api.ui.UiApplication;
import net.rim.device.api.system.Bitmap;
import com.google.zxing.BarcodeFormat;
import com.google.zxing.EncodeHintType;
import com.google.zxing.MultiFormatWriter;
import com.google.zxing.common.BitMatrix;
import java.util.Hashtable;

public final class CreateScreen extends MainScreen {

    private BasicEditField contentField;
    private BitmapField    bitmapField;

    public CreateScreen() {
        super(DEFAULT_CLOSE);
        setTitle("Create QR Code");

        add(new LabelField("Content:"));
        contentField = new BasicEditField("", "", 500, BasicEditField.EDITABLE);
        add(contentField);

        ButtonField genBtn = new ButtonField("Generate QR Code", ButtonField.CONSUME_CLICK);
        genBtn.setChangeListener(new net.rim.device.api.ui.FieldChangeListener() {
            public void fieldChanged(net.rim.device.api.ui.Field field, int context) {
                generateQR();
            }
        });
        add(genBtn);

        bitmapField = new BitmapField(null, BitmapField.FOCUSABLE);
        add(bitmapField);
    }

    private void generateQR() {
        final String text = contentField.getText().trim();
        if (text.length() == 0) {
            Dialog.alert("Content cannot be empty.");
            return;
        }
        new Thread(new Runnable() {
            public void run() {
                final Bitmap result = encode(text, 200);
                UiApplication.getUiApplication().invokeLater(new Runnable() {
                    public void run() {
                        if (result != null) {
                            bitmapField.setBitmap(result);
                        } else {
                            Dialog.alert("Failed to generate QR code.");
                        }
                    }
                });
            }
        }).start();
    }

    private Bitmap encode(String content, int size) {
        try {
            Hashtable hints = new Hashtable();
            hints.put(EncodeHintType.CHARACTER_SET, "UTF-8");
            hints.put(EncodeHintType.MARGIN, new Integer(2));
            MultiFormatWriter writer = new MultiFormatWriter();
            BitMatrix matrix = writer.encode(content, BarcodeFormat.QR_CODE, size, size, hints);
            int w = matrix.getWidth();
            int h = matrix.getHeight();
            int[] pixels = new int[w * h];
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    pixels[y * w + x] = matrix.get(x, y) ? 0xFF000000 : 0xFFFFFFFF;
                }
            }
            Bitmap bmp = new Bitmap(w, h);
            bmp.setARGB(pixels, 0, w, 0, 0, w, h);
            return bmp;
        } catch (Exception e) {
            return null;
        }
    }
}
