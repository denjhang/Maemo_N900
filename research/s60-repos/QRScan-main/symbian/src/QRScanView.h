#ifndef QRSCANVIEW_H
#define QRSCANVIEW_H

#include <aknview.h>
#include <ecam.h>
#include <coecntrl.h>

class CQRScanContainer;

class CQRScanView
    : public CAknView
    , public MCameraObserver
{
public:
    static CQRScanView *NewL();
    virtual ~CQRScanView();

    TUid Id() const;
    void HandleCommandL(TInt aCommand);

protected:
    void DoActivateL(const TVwsViewId &aPrevViewId,
                     TUid              aCustomMessageId,
                     const TDesC8     &aCustomMessage);
    void DoDeactivate();

private:
    void ConstructL();
    void StartCameraL();
    void StopCamera();
    void CaptureNext();

    void McaeoReserveComplete(TInt aError);
    void McaeoPowerOnComplete(TInt aError);
    void McaeoSnapComplete(CFbsBitmap &aSnap);
    void McaeoStillImageReady(CFbsBitmap *aBitmap, HBufC8 *aData, TInt aError);
    void McaeoFrameBufferReady(MFrameBuffer *aFrameBuffer, TInt aError) {}
    void McaeoViewFinderFrameReady(CFbsBitmap &aFrame) {}

    CQRScanContainer *iContainer;
    CCamera          *iCamera;
};

class CQRScanContainer : public CCoeControl {
public:
    static CQRScanContainer *NewL(const TRect &aRect);
    virtual ~CQRScanContainer();
    void Draw(const TRect &aRect) const;
private:
    void ConstructL(const TRect &aRect);
};

#endif
