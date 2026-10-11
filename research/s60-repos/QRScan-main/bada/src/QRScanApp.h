#ifndef QRSCANAPP_H_
#define QRSCANAPP_H_

#include <FApp.h>
#include "QRScanForm.h"

class QRScanBadaApp : public Osp::App::Application {
public:
    static Osp::App::Application *CreateInstance(void);
    QRScanBadaApp();
    virtual ~QRScanBadaApp();
    bool OnAppInitializing(Osp::App::AppRegistry &appRegistry);
    bool OnAppTerminating(Osp::App::AppRegistry &appRegistry,
                          bool forcedTermination = false);
};

#endif
