#include "QRScanView.h"
#include <aknviewappui.h>
#include <eikenv.h>
#include <ecam.h>
#include <fbs.h>
#include <e32base.h>

CQRScanView* CQRScanView::NewL() {
    CQRScanView *self = new (ELeave) CQRScanView();
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
}

void CQRScanView::ConstructL() {
    BaseConstructL();
    iContainer = CQRScanContainer::NewL(ClientRect());
}

CQRScanView::~CQRScanView() {
    delete iContainer;
    if (iCamera) {
        iCamera->Release();
        delete iCamera;
    }
}

TUid CQRScanView::Id() const {
    return TUid::Uid(0x0001);
}

void CQRScanView::HandleCommandL(TInt aCommand) {
    AppUi()->HandleCommandL(aCommand);
}

void CQRScanView::DoActivateL(
    const TVwsViewId& /*aPrevViewId*/,
    TUid              /*aCustomMessageId*/,
    const TDesC8&     /*aCustomMessage*/
) {
    if (!iContainer) {
        iContainer = CQRScanContainer::NewL(ClientRect());
        AppUi()->AddToStackL(*this, iContainer);
    }
    StartCameraL();
}

void CQRScanView::DoDeactivate() {
    StopCamera();
    if (iContainer) {
        AppUi()->RemoveFromViewStack(*this, iContainer);
        delete iContainer;
        iContainer = NULL;
    }
}

void CQRScanView::StartCameraL() {
    iCamera = CCamera::NewL(*this, 0);
    iCamera->Reserve();
}

void CQRScanView::StopCamera() {
    if (iCamera) {
        iCamera->StopViewFinder();
        iCamera->PowerOff();
    }
}

void CQRScanView::McaeoReserveComplete(TInt aError) {
    if (aError == KErrNone) iCamera->PowerOn();
}

void CQRScanView::McaeoPowerOnComplete(TInt aError) {
    if (aError != KErrNone) return;
    TSize size(320, 240);
    iCamera->StartViewFinderDirectL(
        *CEikonEnv::Static()->WsSession(),
        *CCoeEnv::Static()->ScreenDevice(),
        *iContainer->DrawableWindow(),
        TRect(TPoint(0, 0), size)
    );
    CaptureNext();
}

void CQRScanView::CaptureNext() {
    if (!iCamera) return;
    TSize size(640, 480);
    TRAP_IGNORE(iCamera->PrepareImageCaptureL(CCamera::EFormatFbsBitmapColor16M, 0));
    iCamera->CaptureImage();
}

void CQRScanView::McaeoSnapComplete(CFbsBitmap &aSnap) {
}

void CQRScanView::McaeoStillImageReady(CFbsBitmap *aBitmap, HBufC8 *aData, TInt aError) {
    if (aError != KErrNone || !aBitmap) return;
    TSize size = aBitmap->SizeInPixels();
    TInt w = size.iWidth, h = size.iHeight;
    TInt pixels = w * h;
    RArray<TUint32> rgbData;
    if (rgbData.Reserve(pixels) != KErrNone) return;
    for (TInt y = 0; y < h; y++) {
        for (TInt x = 0; x < w; x++) {
            TRgb col;
            aBitmap->GetPixel(col, TPoint(x, y));
            rgbData.Append(col.Value());
        }
    }
}
