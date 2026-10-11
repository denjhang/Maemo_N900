package com.hexadecinull.qrscan.ui;

import net.rim.device.api.ui.container.MainScreen;
import net.rim.device.api.ui.component.LabelField;
import net.rim.device.api.ui.component.BasicEditField;
import net.rim.device.api.ui.component.ButtonField;
import net.rim.device.api.ui.component.SeparatorField;
import net.rim.device.api.ui.UiApplication;
import net.rim.device.api.system.Clipboard;
import net.rim.device.api.ui.component.Dialog;

public final class ResultScreen extends MainScreen {

    private final String text;
    private final String format;

    public ResultScreen(String text, String format) {
        super(DEFAULT_CLOSE);
        this.text   = text;
        this.format = format;
        build();
    }

    private void build() {
        setTitle("Result — " + format);

        add(new LabelField("Format: " + format));
        add(new SeparatorField());

        BasicEditField contentField = new BasicEditField("Content: ", text, 2048,
            BasicEditField.READONLY | BasicEditField.EDITABLE);
        add(contentField);

        add(new SeparatorField());

        ButtonField copyBtn = new ButtonField("Copy to Clipboard",
            ButtonField.CONSUME_CLICK);
        copyBtn.setChangeListener(new net.rim.device.api.ui.FieldChangeListener() {
            public void fieldChanged(net.rim.device.api.ui.Field field, int context) {
                Clipboard.getClipboard().put(text);
                Dialog.inform("Copied to clipboard.");
            }
        });
        add(copyBtn);

        boolean isUrl = text.startsWith("http://") || text.startsWith("https://");
        if (isUrl) {
            ButtonField openBtn = new ButtonField("Open in Browser",
                ButtonField.CONSUME_CLICK);
            openBtn.setChangeListener(new net.rim.device.api.ui.FieldChangeListener() {
                public void fieldChanged(net.rim.device.api.ui.Field field, int context) {
                    try {
                        net.rim.blackberry.api.browser.BrowserSession bs =
                            net.rim.blackberry.api.browser.Browser.getDefaultSession();
                        bs.displayPage(text);
                    } catch (Exception e) {
                        Dialog.alert("Cannot open URL: " + e.getMessage());
                    }
                }
            });
            add(openBtn);
        }
    }
}
