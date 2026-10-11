#include "QRScanAppUi.h"
#include "QRScanView.h"
#include <aknviewappui.h>
#include <eikmenub.h>

void CQRScanAppUi::ConstructL() {
    BaseConstructL(EAknEnableSkin | EAknEnableMSK | EAknSingleClickCompatible);
    CQRScanView *view = CQRScanView::NewL();
    CleanupStack::PushL(view);
    AddViewL(view);
    CleanupStack::Pop(view);
    ActivateLocalViewL(view->Id());
}

CQRScanAppUi::~CQRScanAppUi() {}

void CQRScanAppUi::HandleCommandL(TInt aCommand) {
    switch (aCommand) {
    case EEikCmdExit:
    case EAknSoftkeyExit:
        Exit();
        break;
    default:
        break;
    }
}
