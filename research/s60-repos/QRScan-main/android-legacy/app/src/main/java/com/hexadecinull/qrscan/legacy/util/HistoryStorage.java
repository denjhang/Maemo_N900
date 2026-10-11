package com.hexadecinull.qrscan.legacy.util;

import android.content.Context;
import android.content.SharedPreferences;
import java.util.ArrayList;
import java.util.List;

public final class HistoryStorage {

    public static final class Entry {
        public final String content;
        public final String format;
        public final long   timestamp;

        public Entry(String content, String format, long timestamp) {
            this.content   = content;
            this.format    = format;
            this.timestamp = timestamp;
        }
    }

    private static final String PREFS_NAME  = "qrscan_history";
    private static final String KEY_COUNT   = "count";
    private static final int    MAX_ENTRIES = 200;

    private static HistoryStorage instance;
    private final SharedPreferences prefs;

    private HistoryStorage(Context context) {
        prefs = context.getApplicationContext()
            .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
    }

    public static synchronized HistoryStorage getInstance(Context context) {
        if (instance == null) instance = new HistoryStorage(context);
        return instance;
    }

    public void add(String content, String format) {
        int count = prefs.getInt(KEY_COUNT, 0);
        SharedPreferences.Editor ed = prefs.edit();
        String entry = System.currentTimeMillis() + "|" + format + "|" + content;
        for (int i = Math.min(count, MAX_ENTRIES - 2); i >= 0; i--) {
            String val = prefs.getString("e" + i, null);
            if (val != null) ed.putString("e" + (i + 1), val);
        }
        ed.putString("e0", entry);
        ed.putInt(KEY_COUNT, Math.min(count + 1, MAX_ENTRIES));
        ed.apply();
    }

    public List<Entry> getAll() {
        int count = prefs.getInt(KEY_COUNT, 0);
        List<Entry> result = new ArrayList<Entry>(count);
        for (int i = 0; i < count; i++) {
            String raw = prefs.getString("e" + i, null);
            if (raw == null) continue;
            int firstPipe  = raw.indexOf('|');
            int secondPipe = raw.indexOf('|', firstPipe + 1);
            if (firstPipe < 0 || secondPipe < 0) continue;
            long   ts      = Long.parseLong(raw.substring(0, firstPipe));
            String format  = raw.substring(firstPipe + 1, secondPipe);
            String content = raw.substring(secondPipe + 1);
            result.add(new Entry(content, format, ts));
        }
        return result;
    }

    public void clear() {
        prefs.edit().clear().apply();
    }
}
