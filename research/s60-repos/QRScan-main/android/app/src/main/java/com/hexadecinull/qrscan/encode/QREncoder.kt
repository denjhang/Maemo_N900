package com.hexadecinull.qrscan.encode

import android.graphics.Bitmap
import android.graphics.Color
import com.google.zxing.BarcodeFormat
import com.google.zxing.EncodeHintType
import com.google.zxing.MultiFormatWriter
import com.google.zxing.WriterException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

data class EncodeOptions(
    val content: String,
    val format: BarcodeFormat = BarcodeFormat.QR_CODE,
    val width: Int = 512,
    val height: Int = 512,
    val foregroundColor: Int = Color.BLACK,
    val backgroundColor: Int = Color.WHITE,
    val margin: Int = 2,
    val errorCorrectionLevel: String = "M"
)

object QREncoder {

    suspend fun encode(options: EncodeOptions): Bitmap? = withContext(Dispatchers.Default) {
        try {
            val hints = mapOf(
                EncodeHintType.CHARACTER_SET        to "UTF-8",
                EncodeHintType.MARGIN               to options.margin,
                EncodeHintType.ERROR_CORRECTION     to options.errorCorrectionLevel,
            )
            val writer = MultiFormatWriter()
            val matrix = writer.encode(
                options.content,
                options.format,
                options.width,
                options.height,
                hints
            )

            val pixels = IntArray(options.width * options.height) { idx ->
                val x = idx % options.width
                val y = idx / options.width
                if (matrix[x, y]) options.foregroundColor else options.backgroundColor
            }

            Bitmap.createBitmap(options.width, options.height, Bitmap.Config.ARGB_8888).also {
                it.setPixels(pixels, 0, options.width, 0, 0, options.width, options.height)
            }
        } catch (_: WriterException) {
            null
        }
    }

    val supportedFormats = listOf(
        BarcodeFormat.QR_CODE,
        BarcodeFormat.DATA_MATRIX,
        BarcodeFormat.AZTEC,
        BarcodeFormat.PDF_417,
        BarcodeFormat.CODE_128,
        BarcodeFormat.CODE_39,
        BarcodeFormat.CODE_93,
        BarcodeFormat.EAN_13,
        BarcodeFormat.EAN_8,
        BarcodeFormat.UPC_A,
        BarcodeFormat.UPC_E,
        BarcodeFormat.ITF,
        BarcodeFormat.CODABAR,
    )
}
