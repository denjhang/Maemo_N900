#include <FApp.h>
#include <FBase.h>
#include <FSystem.h>
#include <FUi.h>
#include "QRScanApp.h"

using namespace Osp::App;
using namespace Osp::Base;
using namespace Osp::Ui;

QRScanBadaApp::QRScanBadaApp() {}
QRScanBadaApp::~QRScanBadaApp() {}

Application *QRScanBadaApp::CreateInstance(void) {
    return new QRScanBadaApp();
}

bool QRScanBadaApp::OnAppInitializing(AppRegistry &appRegistry) {
    Frame *pFrame = new Frame();
    pFrame->Construct();
    SetDefaultFrameN(*pFrame);

    QRScanForm *pForm = new QRScanForm();
    pForm->Initialize();
    pFrame->AddControl(*pForm);
    pFrame->SetCurrentForm(*pForm);
    pFrame->Show();

    return true;
}

bool QRScanBadaApp::OnAppTerminating(AppRegistry &appRegistry, bool forcedTermination) {
    return true;
}

extern "C" {
    _EXPORT_ int OspMain(int argc, char *pArgv[]) {
        result r = E_SUCCESS;
        AppLog("QRScan starting");
        r = Osp::App::Application::Execute(QRScanBadaApp::CreateInstance, argc, pArgv);
        TryLog(r == E_SUCCESS, "[%s] Application::Execute failed", GetErrorMessage(r));
        return static_cast<int>(r);
    }
}
