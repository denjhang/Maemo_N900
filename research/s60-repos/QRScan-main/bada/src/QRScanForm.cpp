#include "QRScanForm.h"
#include "QRDecoder.h"
#include <FBase.h>
#include <FGraphics.h>
#include <FMedia.h>
#include <FUi.h>

using namespace Osp::Base;
using namespace Osp::Graphics;
using namespace Osp::Media;
using namespace Osp::Ui;
using namespace Osp::Ui::Controls;

QRScanForm::QRScanForm()
    : __pCamera(null)
    , __pOverlayPanel(null)
    , __pFlashButton(null)
    , __pPickButton(null)
    , __flashOn(false)
    , __scanning(false)
{}

QRScanForm::~QRScanForm() {
    if (__pCamera) {
        __pCamera->PowerOff();
        delete __pCamera;
    }
}

result QRScanForm::Initialize() {
    result r = Form::Construct(FORM_STYLE_NORMAL | FORM_STYLE_TITLE | FORM_STYLE_SOFTKEY_0);
    TryReturn(!IsFailed(r), r, "Form construct failed");

    SetTitleText(L"QRScan");

    r = InitCamera();
    TryReturn(!IsFailed(r), r, "Camera init failed");

    r = BuildUI();
    TryReturn(!IsFailed(r), r, "UI build failed");

    return E_SUCCESS;
}

result QRScanForm::InitCamera() {
    __pCamera = new Camera();
    result r = __pCamera->Construct(*this);
    TryReturn(!IsFailed(r), r, "Camera construct failed");

    r = __pCamera->PowerOn();
    TryReturn(!IsFailed(r), r, "Camera power on failed");

    __pOverlayPanel = new OverlayPanel();
    r = __pOverlayPanel->Construct(Rectangle(0, 0, GetWidth(), GetHeight() - 120));
    TryReturn(!IsFailed(r), r, "OverlayPanel construct failed");

    AddControl(*__pOverlayPanel);

    BufferInfo bufInfo;
    __pOverlayPanel->GetBackgroundBufferInfo(bufInfo);
    __pCamera->StartPreview(&bufInfo, false);

    return E_SUCCESS;
}

result QRScanForm::BuildUI() {
    int w = GetWidth();
    int h = GetHeight();

    __pFlashButton = new Button();
    __pFlashButton->Construct(Rectangle(10, h - 110, (w / 2) - 15, 44), L"Flash");
    __pFlashButton->SetActionId(ID_FLASH);
    __pFlashButton->AddActionEventListener(*this);
    AddControl(*__pFlashButton);

    __pPickButton = new Button();
    __pPickButton->Construct(Rectangle(w / 2 + 5, h - 110, (w / 2) - 15, 44), L"Pick Image");
    __pPickButton->SetActionId(ID_PICK);
    __pPickButton->AddActionEventListener(*this);
    AddControl(*__pPickButton);

    Label *pHint = new Label();
    pHint->Construct(Rectangle(0, h - 60, w, 40), L"Point camera at a QR code");
    pHint->SetTextHorizontalAlignment(ALIGNMENT_CENTER);
    AddControl(*pHint);

    SetSoftkeyText(SOFTKEY_0, L"History");
    SetSoftkeyActionId(SOFTKEY_0, ID_HISTORY);
    AddSoftkeyActionListener(SOFTKEY_0, *this);

    __scanning = true;
    StartDecodeTimer();

    return E_SUCCESS;
}

void QRScanForm::OnActionPerformed(const Control &source, int actionId) {
    switch (actionId) {
    case ID_FLASH:
        ToggleFlash();
        break;
    case ID_PICK:
        PickImage();
        break;
    case ID_HISTORY:
        break;
    }
}

void QRScanForm::ToggleFlash() {
    if (!__pCamera) return;
    __flashOn = !__flashOn;
    __pCamera->SetFlash(__flashOn ? CAMERA_FLASH_ON : CAMERA_FLASH_OFF);
    __pFlashButton->SetText(__flashOn ? L"Flash OFF" : L"Flash ON");
}

void QRScanForm::PickImage() {
    Osp::Ui::Controls::Gallery *pGallery = new Gallery();
    if (pGallery->Construct() == E_SUCCESS) {
        delete pGallery;
    }
}

void QRScanForm::StartDecodeTimer() {
    __pTimer = new Osp::Base::Runtime::Timer();
    __pTimer->Construct(*this);
    __pTimer->StartAsRepeatable(1000);
}

void QRScanForm::OnTimerExpired(Osp::Base::Runtime::Timer &timer) {
    if (!__scanning || !__pCamera) return;
    Osp::Base::ByteBuffer *pBuffer = null;
    __pCamera->Capture();
}

void QRScanForm::OnCameraCaptured(ByteBuffer &capturedData, result r) {
    if (IsFailed(r)) return;
    String result = QRDecoder::DecodeBuffer(capturedData);
    if (!result.IsEmpty()) {
        __scanning = false;
        ShowResult(result, L"QR_CODE");
    }
}

void QRScanForm::ShowResult(const String &text, const String &format) {
}
