package com.hexadecinull.qrscan.decode

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Matrix
import android.net.Uri
import androidx.exifinterface.media.ExifInterface
import com.google.zxing.BinaryBitmap
import com.google.zxing.DecodeHintType
import com.google.zxing.MultiFormatReader
import com.google.zxing.RGBLuminanceSource
import com.google.zxing.Result
import com.google.zxing.common.HybridBinarizer
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

object StaticImageDecoder {

    private const val MAX_PIXELS = 1_500_000

    suspend fun decode(context: Context, uri: Uri): Result? = withContext(Dispatchers.IO) {
        val bitmap = loadAndCorrectBitmap(context, uri) ?: return@withContext null
        decodeBitmap(bitmap)
    }

    fun decodeBitmap(bitmap: Bitmap): Result? {
        val width  = bitmap.width
        val height = bitmap.height
        val pixels = IntArray(width * height)
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height)

        val source = RGBLuminanceSource(width, height, pixels)
        val bmp    = BinaryBitmap(HybridBinarizer(source))

        val reader = MultiFormatReader().apply {
            setHints(
                mapOf(
                    DecodeHintType.TRY_HARDER   to true,
                    DecodeHintType.ALSO_INVERTED to true,
                    DecodeHintType.CHARACTER_SET to "UTF-8",
                )
            )
        }

        return try {
            reader.decode(bmp)
        } catch (_: Exception) {
            null
        }
    }

    private fun loadAndCorrectBitmap(context: Context, uri: Uri): Bitmap? {
        val inputStream = context.contentResolver.openInputStream(uri) ?: return null
        val raw = BitmapFactory.decodeStream(inputStream) ?: return null
        inputStream.close()

        val scaled = scaleBitmap(raw)

        val exifStream = context.contentResolver.openInputStream(uri) ?: return scaled
        val exif       = ExifInterface(exifStream)
        exifStream.close()

        val orientation = exif.getAttributeInt(
            ExifInterface.TAG_ORIENTATION,
            ExifInterface.ORIENTATION_NORMAL
        )

        return rotateBitmap(scaled, orientation)
    }

    private fun scaleBitmap(src: Bitmap): Bitmap {
        val pixels = src.width * src.height
        if (pixels <= MAX_PIXELS) return src
        val scale = Math.sqrt(MAX_PIXELS.toDouble() / pixels).toFloat()
        val w = (src.width * scale).toInt()
        val h = (src.height * scale).toInt()
        return Bitmap.createScaledBitmap(src, w, h, true)
    }

    private fun rotateBitmap(bmp: Bitmap, orientation: Int): Bitmap {
        val matrix = Matrix()
        when (orientation) {
            ExifInterface.ORIENTATION_ROTATE_90  -> matrix.postRotate(90f)
            ExifInterface.ORIENTATION_ROTATE_180 -> matrix.postRotate(180f)
            ExifInterface.ORIENTATION_ROTATE_270 -> matrix.postRotate(270f)
            ExifInterface.ORIENTATION_FLIP_HORIZONTAL -> matrix.preScale(-1f, 1f)
            ExifInterface.ORIENTATION_FLIP_VERTICAL   -> matrix.preScale(1f, -1f)
            else -> return bmp
        }
        return Bitmap.createBitmap(bmp, 0, 0, bmp.width, bmp.height, matrix, true)
    }
}
