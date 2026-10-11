#include "QRScanApp.h"
#include "QRScanAppUi.h"
#include <eikstart.h>
#include <akndoc.h>

class CQRScanDocument : public CAknDocument {
public:
    static CQRScanDocument *NewL(CEikApplication &aApp) {
        CQRScanDocument *self = new (ELeave) CQRScanDocument(aApp);
        return self;
    }
    CEikAppUi *CreateAppUiL() {
        CEikAppUi *appUi = new (ELeave) CQRScanAppUi;
        return appUi;
    }
private:
    CQRScanDocument(CEikApplication &aApp) : CAknDocument(aApp) {}
};

CApaDocument *CQRScanApp::CreateDocumentL() {
    return CQRScanDocument::NewL(*this);
}

TUid CQRScanApp::AppDllUid() const {
    return KUidQRScanApp;
}

static CApaApplication *NewApplication() {
    return new CQRScanApp;
}

GLDEF_C TInt E32Main() {
    return EikStart::RunApplication(NewApplication);
}
