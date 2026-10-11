package com.hexadecinull.qrscan.decode

import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import com.google.zxing.BinaryBitmap
import com.google.zxing.DecodeHintType
import com.google.zxing.MultiFormatReader
import com.google.zxing.PlanarYUVLuminanceSource
import com.google.zxing.Result
import com.google.zxing.common.HybridBinarizer
import java.nio.ByteBuffer

class QRAnalyzer(private val onResult: (Result) -> Unit) : ImageAnalysis.Analyzer {

    private val reader = MultiFormatReader().apply {
        setHints(
            mapOf(
                DecodeHintType.TRY_HARDER       to true,
                DecodeHintType.ALSO_INVERTED     to true,
                DecodeHintType.CHARACTER_SET     to "UTF-8",
            )
        )
    }

    override fun analyze(image: ImageProxy) {
        val buffer: ByteBuffer = image.planes[0].buffer
        val data = ByteArray(buffer.remaining())
        buffer.get(data)

        val width  = image.width
        val height = image.height

        val source = PlanarYUVLuminanceSource(
            data, width, height,
            0, 0, width, height,
            false
        )

        val bmp = BinaryBitmap(HybridBinarizer(source))
        try {
            val result = reader.decodeWithState(bmp)
            onResult(result)
        } catch (_: Exception) {
        } finally {
            reader.reset()
            image.close()
        }
    }
}
