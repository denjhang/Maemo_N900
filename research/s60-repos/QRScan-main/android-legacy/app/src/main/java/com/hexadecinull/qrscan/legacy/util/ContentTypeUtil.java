package com.hexadecinull.qrscan.legacy.util;

public final class ContentTypeUtil {

    private ContentTypeUtil() {}

    public static String resolve(String content) {
        if (content == null || content.length() == 0) return "Plain Text";
        String t = content.trim();
        String low = t.toLowerCase();
        if (low.startsWith("wifi:"))            return "Wi-Fi Network";
        if (low.startsWith("begin:vcard"))      return "Contact (vCard)";
        if (low.startsWith("mecard:"))          return "Contact (MECARD)";
        if (low.startsWith("begin:vcalendar"))  return "Calendar Event";
        if (low.startsWith("geo:"))             return "Geographic Location";
        if (low.startsWith("mailto:"))          return "Email Address";
        if (low.startsWith("tel:"))             return "Phone Number";
        if (low.startsWith("sms"))              return "SMS";
        if (low.startsWith("bitcoin:"))         return "Bitcoin Address";
        if (low.startsWith("http://") ||
            low.startsWith("https://"))         return "URL";
        return "Plain Text";
    }
}
