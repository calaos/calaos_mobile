#ifndef SECRETWIPE_H
#define SECRETWIPE_H

#include <functional>
#include "AdminBiometrics.h"
#include "HardwareUtils.h"
#include "TlsPinning.h"

//What each deliberate reset erases, out of Application so that
//tst_adminrelay runs the same code. Application::logout() is not here: every
//dropped session goes through it, and a stored password must survive those.
namespace SecretWipe
{

inline void userLogout(HardwareUtils *hw, AdminBiometrics *biometrics, const std::function<void()> &logout)
{
    //Persisted: the next launches stay logged out until a login succeeds
    //(homeLoaded()), see LaunchLogin (FIX-11).
    hw->setSessionActive(false);
    //logout() first: it hands a tap taken but not opened yet back to
    //HardwareUtils (DeferredNotifGate::loggedOut() also makes any armed delay
    //stale), so the line below drops it too (FIX-7).
    logout();
    hw->forgetPendingUserActions();
    if (biometrics)
        biometrics->forgetAll();
}

inline void resetCredentials(HardwareUtils *hw, AdminBiometrics *biometrics)
{
    //Also forgets a persisted logout (FIX-11): back to the state of a fresh
    //install, where the demo account is not logged in automatically.
    hw->resetAuthKeychain();
    if (biometrics)
        biometrics->forgetAll();
}

inline void forgetTlsPin(TlsPinStore *pins, AdminBiometrics *biometrics, const QString &host)
{
    if (pins->forget(host))
        qInfo() << "TLS: pin forgotten for" << host;
    if (biometrics)
        biometrics->forgetBox(host);
}

}

#endif // SECRETWIPE_H
