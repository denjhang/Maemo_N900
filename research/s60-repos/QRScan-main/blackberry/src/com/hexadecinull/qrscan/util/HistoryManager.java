package com.hexadecinull.qrscan.util;

import net.rim.device.api.system.PersistentObject;
import net.rim.device.api.system.PersistentStore;
import java.util.Vector;

public final class HistoryManager {

    private static final long KEY = 0xCAFEBABEDEAD0001L;
    private static HistoryManager instance;
    private Vector entries;
    private PersistentObject store;

    private HistoryManager() {
        store = PersistentStore.getPersistentObject(KEY);
        synchronized (store) {
            Object data = store.getContents();
            entries = (data instanceof Vector) ? (Vector) data : new Vector();
        }
    }

    public static synchronized HistoryManager getInstance() {
        if (instance == null) instance = new HistoryManager();
        return instance;
    }

    public void add(String text, String format) {
        String entry = format + "|" + text;
        entries.insertElementAt(entry, 0);
        if (entries.size() > 200) entries.removeElementAt(entries.size() - 1);
        persist();
    }

    public Vector getEntries() {
        return entries;
    }

    public void clear() {
        entries.removeAllElements();
        persist();
    }

    private void persist() {
        synchronized (store) {
            store.setContents(entries);
            store.commit();
        }
    }
}
