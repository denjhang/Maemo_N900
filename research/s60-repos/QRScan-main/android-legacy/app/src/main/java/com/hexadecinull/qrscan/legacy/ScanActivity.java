package com.hexadecinull.qrscan.legacy;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.hardware.Camera;
import android.os.Bundle;
import android.os.Handler;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.Toast;
import com.hexadecinull.qrscan.legacy.camera.CameraManager;
import com.hexadecinull.qrscan.legacy.camera.PreviewCallback;
import com.hexadecinull.qrscan.legacy.decode.LegacyDecodeThread;
import com.hexadecinull.qrscan.legacy.decode.DecodeResult;

public class ScanActivity extends Activity implements SurfaceHolder.Callback, PreviewCallback.Listener {

    private CameraManager cameraManager;
    private SurfaceView surfaceView;
    private SurfaceHolder surfaceHolder;
    private ImageButton btnFlash;
    private ImageButton btnHistory;
    private ImageButton btnCreate;
    private boolean flashOn = false;
    private boolean hasSurface = false;
    private LegacyDecodeThread decodeThread;
    private Handler handler;
    private boolean paused = false;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Window window = getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_scan);

        cameraManager = new CameraManager(getApplication());
        surfaceView   = (SurfaceView) findViewById(R.id.preview_view);
        btnFlash      = (ImageButton) findViewById(R.id.btn_flash);
        btnHistory    = (ImageButton) findViewById(R.id.btn_history);
        btnCreate     = (ImageButton) findViewById(R.id.btn_create);
        handler       = new Handler();

        if (getIntent().getAction() != null &&
                getIntent().getAction().equals(Intent.ACTION_SEND)) {
            android.net.Uri uri = getIntent().getParcelableExtra(Intent.EXTRA_STREAM);
            if (uri != null) {
                decodeFromUri(uri);
                return;
            }
        }

        btnFlash.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                flashOn = !flashOn;
                cameraManager.setTorch(flashOn);
                btnFlash.setImageResource(flashOn
                    ? R.drawable.ic_flash_on
                    : R.drawable.ic_flash_off);
            }
        });

        btnHistory.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startActivity(new Intent(ScanActivity.this, HistoryActivity.class));
            }
        });

        btnCreate.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startActivity(new Intent(ScanActivity.this, CreateActivity.class));
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        paused = false;
        SurfaceHolder holder = surfaceView.getHolder();
        if (hasSurface) {
            initCamera(holder);
        } else {
            holder.addCallback(this);
            holder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        paused = true;
        if (decodeThread != null) {
            decodeThread.quit();
            decodeThread = null;
        }
        cameraManager.stopPreview();
        cameraManager.closeDriver();
        if (!hasSurface) {
            surfaceView.getHolder().removeCallback(this);
        }
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        if (!hasSurface) {
            hasSurface = true;
            initCamera(holder);
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        hasSurface = false;
    }

    private void initCamera(SurfaceHolder holder) {
        if (cameraManager.isOpen()) return;
        try {
            cameraManager.openDriver(holder);
            cameraManager.startPreview();
            restartDecode();
        } catch (Exception e) {
            Toast.makeText(this, getString(R.string.camera_error), Toast.LENGTH_SHORT).show();
        }
    }

    private void restartDecode() {
        if (paused) return;
        decodeThread = new LegacyDecodeThread(cameraManager, this);
        decodeThread.start();
    }

    @Override
    public void onDecodeResult(final DecodeResult result) {
        handler.post(new Runnable() {
            @Override
            public void run() {
                if (!paused) {
                    Intent intent = new Intent(ScanActivity.this, ResultActivity.class);
                    intent.putExtra(ResultActivity.EXTRA_CONTENT, result.getText());
                    intent.putExtra(ResultActivity.EXTRA_FORMAT, result.getFormatName());
                    startActivity(intent);
                }
            }
        });
    }

    private void decodeFromUri(android.net.Uri uri) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                DecodeResult result = com.hexadecinull.qrscan.legacy.decode.StaticDecoder.decode(
                    ScanActivity.this, uri);
                if (result != null) {
                    Intent intent = new Intent(ScanActivity.this, ResultActivity.class);
                    intent.putExtra(ResultActivity.EXTRA_CONTENT, result.getText());
                    intent.putExtra(ResultActivity.EXTRA_FORMAT, result.getFormatName());
                    startActivity(intent);
                } else {
                    handler.post(new Runnable() {
                        @Override
                        public void run() {
                            Toast.makeText(ScanActivity.this,
                                getString(R.string.no_qr_found), Toast.LENGTH_SHORT).show();
                        }
                    });
                }
            }
        }).start();
    }
}
