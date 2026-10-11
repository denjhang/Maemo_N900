#ifndef QRSCANFORM_H_
#define QRSCANFORM_H_

#include <FBase.h>
#include <FMedia.h>
#include <FUi.h>

class QRScanForm
    : public Osp::Ui::Controls::Form
    , public Osp::Ui::IActionEventListener
    , public Osp::Media::ICameraEventListener
    , public Osp::Base::Runtime::ITimerEventListener
{
public:
    static const int ID_FLASH   = 100;
    static const int ID_PICK    = 101;
    static const int ID_HISTORY = 102;

    QRScanForm();
    virtual ~QRScanForm();
    result Initialize();

    virtual void OnActionPerformed(const Osp::Ui::Control &source, int actionId);
    virtual void OnCameraCaptured(Osp::Base::ByteBuffer &capturedData, result r);
    virtual void OnCameraAutoFocused(bool success) {}
    virtual void OnCameraPreviewed(Osp::Base::ByteBuffer &previewedData, result r) {}
    virtual void OnCameraErrorOccurred(Osp::Media::CameraErrorReason r) {}
    virtual void OnTimerExpired(Osp::Base::Runtime::Timer &timer);

private:
    result InitCamera();
    result BuildUI();
    void   ToggleFlash();
    void   PickImage();
    void   StartDecodeTimer();
    void   ShowResult(const Osp::Base::String &text, const Osp::Base::String &format);

    Osp::Media::Camera                     *__pCamera;
    Osp::Ui::Controls::OverlayPanel        *__pOverlayPanel;
    Osp::Ui::Controls::Button              *__pFlashButton;
    Osp::Ui::Controls::Button              *__pPickButton;
    Osp::Base::Runtime::Timer              *__pTimer;
    bool                                    __flashOn;
    bool                                    __scanning;
};

#endif
