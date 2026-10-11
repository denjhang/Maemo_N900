package com.hexadecinull.qrscan.legacy;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

public class SettingsActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(android.R.layout.simple_list_item_1);
        if (getActionBar() != null) {
            getActionBar().setTitle("Settings");
            getActionBar().setDisplayHomeAsUpEnabled(true);
        }
        TextView tv = (TextView) findViewById(android.R.id.text1);
        tv.setText(getString(R.string.about));
        tv.setPadding(32, 32, 32, 32);
    }
}
