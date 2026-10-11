package com.hexadecinull.qrscan.legacy.decode;

public final class DecodeResult {
    private final String text;
    private final String formatName;

    public DecodeResult(String text, String formatName) {
        this.text = text;
        this.formatName = formatName;
    }

    public String getText() { return text; }
    public String getFormatName() { return formatName; }
}
