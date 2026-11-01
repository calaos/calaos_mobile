#ifndef RESUMEPOLICY_H
#define RESUMEPOLICY_H

//What the app keeps on screen across an iOS resign-active / become-active
//cycle.
//
//iOS drops the session when the app resigns active (FIX-7), which pops the
//whole page stack to the login screen; the automatic login on return then
//opens the home page. Resign-active is also sent when the user leaves for the
//password manager (1Password, iCloud Keychain) to copy a password, so the page
//waiting for that password was gone on return. While a page must survive (the
//box updates page is shown), the session still drops but the page stays and
//the automatic login on return reconnects in place; if that login fails, the
//usual return to the login screen happens.
//
//Header-only, no QObject: tests drive it without Application.
class ResumePolicy
{
public:
    enum Phase
    {
        None,     //nothing kept
        Paused,   //in the background, the page is kept
        Resuming, //back, the automatic login is on its way
    };

    //The app resigns active. keepPage: the page shown must survive;
    //loginEdited: the user has typed in the login fields, the automatic login
    //on return would replace that text with the stored credentials.
    void resigned(bool keepPage, bool loginEdited)
    {
        m_phase = keepPage ? Paused : None;
        m_skipAutoLogin = loginEdited;
    }

    //Back in the foreground. Returns whether the automatic login runs.
    bool becameActive()
    {
        const bool skip = m_skipAutoLogin;
        m_skipAutoLogin = false;
        if (skip)
        {
            m_phase = None;
            return false;
        }
        if (m_phase == Paused)
            m_phase = Resuming;
        return true;
    }

    //The session went down again (failed login, untrusted certificate...)
    //after the return: back to the normal path, the login screen.
    void sessionFailed()
    {
        if (m_phase == Resuming)
            m_phase = None;
    }

    //Logged in again, in place.
    void loggedIn() { m_phase = None; }

    //Logout, reset: nothing kept.
    void reset()
    {
        m_phase = None;
        m_skipAutoLogin = false;
    }

    Phase phase() const { return m_phase; }
    bool keepPage() const { return m_phase != None; }

private:
    Phase m_phase = None;
    bool m_skipAutoLogin = false;
};

#endif // RESUMEPOLICY_H
