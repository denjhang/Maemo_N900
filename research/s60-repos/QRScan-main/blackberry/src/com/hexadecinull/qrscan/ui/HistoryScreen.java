package com.hexadecinull.qrscan.ui;

import net.rim.device.api.ui.container.MainScreen;
import net.rim.device.api.ui.component.LabelField;
import net.rim.device.api.ui.component.SeparatorField;
import net.rim.device.api.ui.component.ListField;
import net.rim.device.api.ui.component.ListFieldCallback;
import net.rim.device.api.ui.Graphics;
import net.rim.device.api.ui.UiApplication;
import com.hexadecinull.qrscan.util.HistoryManager;
import java.util.Vector;

public final class HistoryScreen extends MainScreen {

    private final Vector entries;

    public HistoryScreen() {
        super(DEFAULT_CLOSE);
        setTitle("History");
        entries = HistoryManager.getInstance().getEntries();

        if (entries.isEmpty()) {
            add(new LabelField("No history yet."));
            return;
        }

        ListField list = new ListField(entries.size());
        list.setCallback(new ListFieldCallback() {
            public void drawListRow(ListField lf, Graphics g, int index, int y, int width) {
                String raw = (String) entries.elementAt(index);
                int sep = raw.indexOf('|');
                String display = sep >= 0 ? raw.substring(sep + 1) : raw;
                if (display.length() > 60) display = display.substring(0, 60) + "\u2026";
                g.drawText(display, 4, y, 0, width - 8);
            }
            public Object get(ListField lf, int index) { return entries.elementAt(index); }
            public int getPreferredWidth(ListField lf) { return lf.getWidth(); }
            public int indexOfList(ListField lf, String prefix, int start) { return -1; }
        });
        add(list);
    }
}
