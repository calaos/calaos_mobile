#include "XUtils.h"

#include <QtGlobal>
#include <QDebug>

#ifdef Q_OS_LINUX
#define HAVE_X_DMPS
#elif defined(Q_OS_WIN)
#define HAVE_WIN_DMPS
#endif

#ifdef HAVE_X_DMPS
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
#endif

#ifdef HAVE_WIN_DMPS
#include <qt_windows.h>

const int MONITOR_ON = -1;
const int MONITOR_OFF = 2;
const int MONITOR_STANBY = 1;
#endif

void XUtils::UpdateDPMS(bool enable, int seconds)
{
#ifdef HAVE_X_DMPS
    //X11 display
    Display *x_display;

    x_display = XOpenDisplay(":0");
    if (!x_display)
    {
        qWarning() <<  "XUtils::UpdateDPMS(): trying with $DISPLAY";
        x_display = XOpenDisplay(NULL);

        if (!x_display)
        {
            qWarning() <<  "XUtils::UpdateDPMS(): Error opening X11 display";
            return;
        }
    }

    if (!DPMSCapable(x_display))
    {
        qWarning() <<  "X display is not DPMS capable !";
    }
    else
    {
        if (enable)
        {
            DPMSEnable(x_display);

            int s1 = seconds - 4, s2 = seconds - 2, s3 = seconds;
            if (s1 < 0) s1 = 0;
            if (s2 < 0) s2 = 0;
            if (s3 < 0) s3 = 0;

            //set timeouts
            DPMSSetTimeouts(x_display, s1, s2, s3);
        }
        else
            DPMSDisable(x_display);

        XSetScreenSaver(x_display, 0, 0, 0, 0);
    }

    //Close the X11 connection
    if (x_display)
        XCloseDisplay(x_display);
#else
    Q_UNUSED(enable)
    Q_UNUSED(seconds)
#endif
}

void XUtils::WakeUpScreen(bool enable)
{
    qDebug() << "XUtils::WakeUpScreen(" << enable << ")";
#ifdef HAVE_X_DMPS
    //X11 display
    Display *x_display;

    x_display = XOpenDisplay(":0");
    if (!x_display)
    {
        qWarning() <<  "XUtils::WakeUpScreen(): trying with $DISPLAY";
        x_display = XOpenDisplay(NULL);

        if (!x_display)
        {
            qWarning() <<  "XUtils::WakeUpScreen(): Error opening X11 display";
            return;
        }
    }

    if (!DPMSCapable(x_display))
    {
        qWarning() <<  "X display is not DPMS capable !";
    }
    else
    {
        if (enable)
        {
            DPMSForceLevel(x_display, DPMSModeOn);
        }
        else
        {
            DPMSForceLevel(x_display, DPMSModeOff);
        }
    }

    //Close the X11 connection
    if (x_display)
        XCloseDisplay(x_display);
#elif defined(HAVE_WIN_DMPS)
    //Windows API

//    if (enable)
//        PostMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, MONITOR_ON);
//    else
//        PostMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, MONITOR_OFF);
#else
    Q_UNUSED(enable)
#endif
}

int XUtils::getDPMSInfo()
{
#ifdef HAVE_X_DMPS
    //X11 display
    Display *x_display;
    int ret = DPMS_NOTAVAILABLE;

    x_display = XOpenDisplay(":0");
    if (!x_display)
        qWarning() <<  "XUtils::getDPMSInfo(): Error opening X11 display";

    if (!DPMSCapable(x_display))
    {
        qWarning() <<  "X display is not DPMS capable !";
    }
    else
    {
        CARD16 mode;
        BOOL state;
        DPMSInfo(x_display, &mode, &state);

        if (!state)
            ret = DPMS_DISABLED;
        else
        {
            if (mode == DPMSModeOn) ret = DPMS_ON;
            if (mode == DPMSModeOff) ret = DPMS_OFF;
            if (mode == DPMSModeStandby) ret = DPMS_STANDBY;
            if (mode == DPMSModeSuspend) ret = DPMS_SUSPEND;
        }
    }

    //Close the X11 connection
    if (x_display)
        XCloseDisplay(x_display);

    return ret;
#endif
    return 0;
}
