#include "QRScanView.h"
#include <eikenv.h>

CQRScanContainer *CQRScanContainer::NewL(const TRect &aRect) {
    CQRScanContainer *self = new (ELeave) CQRScanContainer;
    CleanupStack::PushL(self);
    self->ConstructL(aRect);
    CleanupStack::Pop(self);
    return self;
}

void CQRScanContainer::ConstructL(const TRect &aRect) {
    CreateWindowL();
    SetRect(aRect);
    ActivateL();
}

CQRScanContainer::~CQRScanContainer() {}

void CQRScanContainer::Draw(const TRect &aRect) const {
    CWindowGc &gc = SystemGc();
    gc.SetBrushColor(KRgbBlack);
    gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
    gc.DrawRect(aRect);
}
