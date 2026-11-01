#ifndef LAUNCHLOGIN_H
#define LAUNCHLOGIN_H

#include "CalaosConnection.h"
#include "HardwareUtils.h"
#include "ResumePolicy.h"
#include "Common.h"

//What a fresh start of the app does with the session (FIX-11), and the
//automatic login when it comes back to the foreground.
//
//A deliberate logout (Application::userLogout()) is persisted by the platform
//(HardwareUtils::setSessionActive(false)) and survives a restart: the new
//connection starts closed by the user, so no automatic login (the launch
//login, iOS become-active, the network coming back) reopens it. Only an
//explicit login does, and its success clears the persisted state
//(Application::homeLoaded()).
//
//Header-only and without Application, so tests/tst_sessionlifecycle drives
//the same code with a real CalaosConnection.
namespace LaunchLogin
{

//Called once on the connection created at startup, before any login.
inline void restoreUserLogout(CalaosConnection *connection, HardwareUtils *hw)
{
    if (!hw->isSessionActive())
    {
        qDebug() << "Logged out by the user before the restart: no automatic login";
        connection->logout();
    }
}

inline bool isDemoAccount(const QString &user, const QString &pass, const QString &host)
{
    return (user == QLatin1String("demo") || user == Common::getDemoUser()) &&
           pass == Common::getDemoPass() &&
           host == Common::getDemoHost();
}

//The automatic login started ~100 ms after launch on mobile. Not for the
//demo account (the login screen is shown first), nor after a logout.
inline bool shouldAutoLogin(const CalaosConnection *connection,
                            const QString &user, const QString &pass, const QString &host)
{
    if (connection->sessionClosedByUser())
        return false;
    return !isDemoAccount(user, pass, host);
}

//The automatic login when iOS brings the app back. Not after a logout, not
//once the server refused the credentials (one refusal is enough: only the user
//or a successful login lifts it), not over edited login fields (ResumePolicy).
//The demo account is not excluded here: it would drop a demo session the user
//opened himself back to the login screen.
inline bool shouldLoginOnReturn(const CalaosConnection *connection, ResumePolicy &policy)
{
    if (connection->sessionClosedByUser() || connection->failingOnCredentials())
    {
        policy.reset();
        return false;
    }
    return policy.becameActive();
}

}

#endif // LAUNCHLOGIN_H
