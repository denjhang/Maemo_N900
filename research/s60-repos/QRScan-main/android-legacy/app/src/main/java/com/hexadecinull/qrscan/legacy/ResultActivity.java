package com.hexadecinull.qrscan.legacy;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;
import com.hexadecinull.qrscan.legacy.util.ContentTypeUtil;

public class ResultActivity extends Activity {

    public static final String EXTRA_CONTENT = "content";
    public static final String EXTRA_FORMAT  = "format";

    private String content;
    private String format;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_result);

        content = getIntent().getStringExtra(EXTRA_CONTENT);
        format  = getIntent().getStringExtra(EXTRA_FORMAT);
        if (content == null) content = "";
        if (format  == null) format  = "";

        if (getActionBar() != null) {
            getActionBar().setTitle(format);
            getActionBar().setDisplayHomeAsUpEnabled(true);
        }

        TextView tvFormat = (TextView) findViewById(R.id.tv_format);
        tvFormat.setText(format);

        TextView tvContent = (TextView) findViewById(R.id.tv_content);
        tvContent.setText(content);

        TextView tvType = (TextView) findViewById(R.id.tv_content_type);
        tvType.setText(ContentTypeUtil.resolve(content));

        Button btnCopy = (Button) findViewById(R.id.btn_copy);
        btnCopy.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                copyToClipboard();
            }
        });

        Button btnShare = (Button) findViewById(R.id.btn_share);
        btnShare.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                shareContent();
            }
        });

        Button btnOpen = (Button) findViewById(R.id.btn_open);
        if (content.startsWith("http://") || content.startsWith("https://")) {
            btnOpen.setVisibility(View.VISIBLE);
            btnOpen.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    openUrl();
                }
            });
        }

        Button btnCreate = (Button) findViewById(R.id.btn_create);
        btnCreate.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(ResultActivity.this, CreateActivity.class);
                intent.putExtra(CreateActivity.EXTRA_PREFILL, content);
                startActivity(intent);
            }
        });
    }

    private void copyToClipboard() {
        Object service = getSystemService(Context.CLIPBOARD_SERVICE);
        if (service instanceof ClipboardManager) {
            ClipboardManager cm = (ClipboardManager) service;
            cm.setPrimaryClip(ClipData.newPlainText("QRScan", content));
        }
        Toast.makeText(this, R.string.copied, Toast.LENGTH_SHORT).show();
    }

    private void shareContent() {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_TEXT, content);
        startActivity(Intent.createChooser(intent, getString(R.string.share)));
    }

    private void openUrl() {
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(content)));
        } catch (Exception ignored) {
        }
    }
}
