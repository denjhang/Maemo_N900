#include "QRDecoder.h"
#include <FGraphics.h>
#include <FMedia.h>

#include <zxing/MultiFormatReader.h>
#include <zxing/BinaryBitmap.h>
#include <zxing/RGBLuminanceSource.h>
#include <zxing/common/HybridBinarizer.h>
#include <zxing/DecodeHints.h>

using namespace Osp::Base;
using namespace Osp::Graphics;
using namespace Osp::Media;
using namespace zxing;

String QRDecoder::DecodeBuffer(ByteBuffer &jpegBuffer) {
    ImageBuffer imgBuf;
    result r = ImageBuffer::DecodeN(jpegBuffer, BITMAP_PIXEL_FORMAT_ARGB8888, imgBuf);
    if (IsFailed(r)) return String(L"");

    int w = imgBuf.GetWidth();
    int h = imgBuf.GetHeight();
    const byte *data = imgBuf.GetData();

    int *rgbPixels = new int[w * h];
    for (int i = 0; i < w * h; i++) {
        rgbPixels[i] = (((int)data[i * 4 + 3]) << 24) |
                       (((int)data[i * 4 + 0]) << 16) |
                       (((int)data[i * 4 + 1]) << 8)  |
                       (((int)data[i * 4 + 2]));
    }

    try {
        Ref<LuminanceSource> source(new RGBLuminanceSource(rgbPixels, w, h));
        Ref<Binarizer>       binarizer(new HybridBinarizer(source));
        Ref<BinaryBitmap>    bmp(new BinaryBitmap(binarizer));
        DecodeHints hints(DecodeHints::DEFAULT_HINT);
        hints.setTryHarder(true);
        MultiFormatReader reader;
        Ref<Result>       result = reader.decode(bmp, hints);
        delete[] rgbPixels;
        std::string text = result->getText()->getText();
        return String(text.c_str());
    } catch (...) {
        delete[] rgbPixels;
        return String(L"");
    }
}
