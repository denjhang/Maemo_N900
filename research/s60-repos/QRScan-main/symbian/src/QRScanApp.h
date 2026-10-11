#ifndef QRSCANAPP_H
#define QRSCANAPP_H

#include <aknapp.h>

const TUid KUidQRScanApp = { 0xE1234567 };

class CQRScanApp : public CAknApplication {
    CApaDocument *CreateDocumentL();
    TUid AppDllUid() const;
};

#endif
