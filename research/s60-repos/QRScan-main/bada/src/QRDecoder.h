#ifndef QRDECODER_H_
#define QRDECODER_H_

#include <FBase.h>
#include <FGraphics.h>

class QRDecoder {
public:
    static Osp::Base::String DecodeBuffer(Osp::Base::ByteBuffer &jpegBuffer);
    static Osp::Base::String DecodeBitmap(Osp::Graphics::Bitmap &bitmap);
};

#endif
