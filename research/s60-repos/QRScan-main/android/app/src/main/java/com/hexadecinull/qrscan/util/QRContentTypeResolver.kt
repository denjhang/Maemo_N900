package com.hexadecinull.qrscan.util

enum class QRContentType(val label: String) {
    URL("URL"),
    WIFI("Wi-Fi Network"),
    VCARD("Contact (vCard)"),
    MECARD("Contact (MECARD)"),
    GEO("Geographic Location"),
    EMAIL("Email Address"),
    PHONE("Phone Number"),
    SMS("SMS"),
    CALENDAR("Calendar Event"),
    APP("App / Market Link"),
    BITCOIN("Bitcoin Address"),
    TEXT("Plain Text")
}

object QRContentTypeResolver {

    private val urlPattern     = Regex("^https?://.*", RegexOption.IGNORE_CASE)
    private val wifiPattern    = Regex("^WIFI:.*", RegexOption.IGNORE_CASE)
    private val vcardPattern   = Regex("^BEGIN:VCARD.*", RegexOption.IGNORE_CASE)
    private val mecardPattern  = Regex("^MECARD:.*", RegexOption.IGNORE_CASE)
    private val geoPattern     = Regex("^geo:[\\-0-9.]+,[\\-0-9.]+.*", RegexOption.IGNORE_CASE)
    private val mailtoPattern  = Regex("^mailto:.*", RegexOption.IGNORE_CASE)
    private val telPattern     = Regex("^tel:.*", RegexOption.IGNORE_CASE)
    private val smsPattern     = Regex("^smsto?:.*", RegexOption.IGNORE_CASE)
    private val calPattern     = Regex("^BEGIN:VCALENDAR.*", RegexOption.IGNORE_CASE)
    private val marketPattern  = Regex("^market://.*|^https?://play\\.google\\.com/.*", RegexOption.IGNORE_CASE)
    private val bitcoinPattern = Regex("^bitcoin:.*", RegexOption.IGNORE_CASE)

    fun resolve(content: String): QRContentType {
        val trimmed = content.trim()
        return when {
            wifiPattern.containsMatchIn(trimmed)    -> QRContentType.WIFI
            vcardPattern.containsMatchIn(trimmed)   -> QRContentType.VCARD
            mecardPattern.containsMatchIn(trimmed)  -> QRContentType.MECARD
            calPattern.containsMatchIn(trimmed)     -> QRContentType.CALENDAR
            geoPattern.containsMatchIn(trimmed)     -> QRContentType.GEO
            mailtoPattern.containsMatchIn(trimmed)  -> QRContentType.EMAIL
            telPattern.containsMatchIn(trimmed)     -> QRContentType.PHONE
            smsPattern.containsMatchIn(trimmed)     -> QRContentType.SMS
            bitcoinPattern.containsMatchIn(trimmed) -> QRContentType.BITCOIN
            marketPattern.containsMatchIn(trimmed)  -> QRContentType.APP
            urlPattern.containsMatchIn(trimmed)     -> QRContentType.URL
            else                                    -> QRContentType.TEXT
        }
    }
}
