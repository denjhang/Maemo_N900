package com.hexadecinull.qrscan.legacy;

import android.app.Activity;
import android.content.ContentValues;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.Toast;
import com.google.zxing.BarcodeFormat;
import com.google.zxing.EncodeHintType;
import com.google.zxing.MultiFormatWriter;
import com.google.zxing.WriterException;
import com.google.zxing.common.BitMatrix;
import java.io.File;
import java.io.FileOutputStream;
import java.util.Hashtable;

public class CreateActivity extends Activity {

    public static final String EXTRA_PREFILL = "prefill";

    private static final BarcodeFormat[] FORMATS = {
        BarcodeFormat.QR_CODE,
        BarcodeFormat.DATA_MATRIX,
        BarcodeFormat.AZTEC,
        BarcodeFormat.PDF_417,
        BarcodeFormat.CODE_128,
        BarcodeFormat.CODE_39,
        BarcodeFormat.EAN_13,
        BarcodeFormat.EAN_8,
    };

    private EditText etContent;
    private Spinner spinnerFormat;
    private ImageView ivQr;
    private LinearLayout llActions;
    private Bitmap currentBitmap;
    private BarcodeFormat selectedFormat = BarcodeFormat.QR_CODE;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_create);

        if (getActionBar() != null) {
            getActionBar().setTitle(R.string.create);
            getActionBar().setDisplayHomeAsUpEnabled(true);
        }

        etContent     = (EditText)      findViewById(R.id.et_content);
        spinnerFormat = (Spinner)       findViewById(R.id.spinner_format);
        ivQr          = (ImageView)     findViewById(R.id.iv_qr);
        llActions     = (LinearLayout)  findViewById(R.id.ll_actions);

        String prefill = getIntent().getStringExtra(EXTRA_PREFILL);
        if (prefill != null) etContent.setText(prefill);

        String[] names = new String[FORMATS.length];
        for (int i = 0; i < FORMATS.length; i++) {
            names[i] = FORMATS[i].name().replace('_', ' ');
        }
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(
            this, android.R.layout.simple_spinner_item, names);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinnerFormat.setAdapter(adapter);
        spinnerFormat.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
                selectedFormat = FORMATS[pos];
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });

        Button btnGenerate = (Button) findViewById(R.id.btn_generate);
        btnGenerate.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                generate();
            }
        });

        Button btnSave = (Button) findViewById(R.id.btn_save);
        btnSave.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                saveToGallery();
            }
        });

        Button btnShare = (Button) findViewById(R.id.btn_share);
        btnShare.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                shareImage();
            }
        });
    }

    private void generate() {
        String text = etContent.getText().toString().trim();
        if (text.isEmpty()) {
            Toast.makeText(this, "Content cannot be empty.", Toast.LENGTH_SHORT).show();
            return;
        }
        new Thread(new Runnable() {
            @Override
            public void run() {
                final Bitmap bmp = encode(text, selectedFormat, 512);
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (bmp != null) {
                            currentBitmap = bmp;
                            ivQr.setImageBitmap(bmp);
                            ivQr.setVisibility(View.VISIBLE);
                            llActions.setVisibility(View.VISIBLE);
                        } else {
                            Toast.makeText(CreateActivity.this,
                                "Cannot encode this content as " + selectedFormat.name(),
                                Toast.LENGTH_SHORT).show();
                        }
                    }
                });
            }
        }).start();
    }

    private Bitmap encode(String content, BarcodeFormat format, int size) {
        try {
            Hashtable<EncodeHintType, Object> hints = new Hashtable<EncodeHintType, Object>();
            hints.put(EncodeHintType.CHARACTER_SET, "UTF-8");
            hints.put(EncodeHintType.MARGIN, 2);
            MultiFormatWriter writer = new MultiFormatWriter();
            BitMatrix matrix = writer.encode(content, format, size, size, hints);
            int w = matrix.getWidth();
            int h = matrix.getHeight();
            int[] pixels = new int[w * h];
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    pixels[y * w + x] = matrix.get(x, y) ? Color.BLACK : Color.WHITE;
                }
            }
            Bitmap bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
            bmp.setPixels(pixels, 0, w, 0, 0, w, h);
            return bmp;
        } catch (WriterException e) {
            return null;
        }
    }

    private void saveToGallery() {
        if (currentBitmap == null) return;
        try {
            String filename = "QRScan_" + System.currentTimeMillis() + ".png";
            File dir = new File(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES),
                "QRScan");
            dir.mkdirs();
            File file = new File(dir, filename);
            FileOutputStream fos = new FileOutputStream(file);
            currentBitmap.compress(Bitmap.CompressFormat.PNG, 100, fos);
            fos.close();
            sendBroadcast(new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE,
                Uri.fromFile(file)));
            Toast.makeText(this, R.string.saved, Toast.LENGTH_SHORT).show();
        } catch (Exception e) {
            Toast.makeText(this, R.string.save_failed, Toast.LENGTH_SHORT).show();
        }
    }

    private void shareImage() {
        if (currentBitmap == null) return;
        try {
            File dir = new File(getCacheDir(), "shared_qr");
            dir.mkdirs();
            File file = new File(dir, "qrscan_share.png");
            FileOutputStream fos = new FileOutputStream(file);
            currentBitmap.compress(Bitmap.CompressFormat.PNG, 100, fos);
            fos.close();
            Intent intent = new Intent(Intent.ACTION_SEND);
            intent.setType("image/png");
            intent.putExtra(Intent.EXTRA_STREAM, Uri.fromFile(file));
            startActivity(Intent.createChooser(intent, getString(R.string.share)));
        } catch (Exception ignored) {
        }
    }
}
