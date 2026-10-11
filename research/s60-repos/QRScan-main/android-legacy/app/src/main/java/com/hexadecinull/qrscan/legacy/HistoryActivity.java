package com.hexadecinull.qrscan.legacy;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;
import com.hexadecinull.qrscan.legacy.util.HistoryStorage;
import java.util.List;

public class HistoryActivity extends Activity {

    private ListView lvHistory;
    private TextView tvEmpty;
    private List<HistoryStorage.Entry> entries;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_history);

        if (getActionBar() != null) {
            getActionBar().setTitle(R.string.history);
            getActionBar().setDisplayHomeAsUpEnabled(true);
        }

        lvHistory = (ListView) findViewById(R.id.lv_history);
        tvEmpty   = (TextView) findViewById(R.id.tv_empty);

        loadHistory();

        lvHistory.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int pos, long id) {
                HistoryStorage.Entry entry = entries.get(pos);
                Intent intent = new Intent(HistoryActivity.this, ResultActivity.class);
                intent.putExtra(ResultActivity.EXTRA_CONTENT, entry.content);
                intent.putExtra(ResultActivity.EXTRA_FORMAT, entry.format);
                startActivity(intent);
            }
        });
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        menu.add(0, 1, 0, R.string.clear_history);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == 1) {
            HistoryStorage.getInstance(this).clear();
            loadHistory();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void loadHistory() {
        entries = HistoryStorage.getInstance(this).getAll();
        if (entries.isEmpty()) {
            lvHistory.setVisibility(View.GONE);
            tvEmpty.setVisibility(View.VISIBLE);
        } else {
            tvEmpty.setVisibility(View.GONE);
            lvHistory.setVisibility(View.VISIBLE);
            String[] labels = new String[entries.size()];
            for (int i = 0; i < entries.size(); i++) {
                String c = entries.get(i).content;
                labels[i] = (c.length() > 60 ? c.substring(0, 60) + "\u2026" : c)
                    + "\n" + entries.get(i).format;
            }
            lvHistory.setAdapter(new ArrayAdapter<String>(
                this, R.layout.list_item_scan, R.id.tv_content, labels));
        }
    }
}
