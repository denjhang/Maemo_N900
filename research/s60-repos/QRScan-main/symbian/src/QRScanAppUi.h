#ifndef QRSCANAPPUI_H
#define QRSCANAPPUI_H

#include <aknviewappui.h>

class CQRScanAppUi : public CAknViewAppUi {
public:
    void ConstructL();
    virtual ~CQRScanAppUi();
    void HandleCommandL(TInt aCommand);
};

#endif
