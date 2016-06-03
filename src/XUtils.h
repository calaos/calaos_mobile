#ifndef XUTILS_H
#define XUTILS_H

class XUtils
{
public:
    enum { DPMS_NOTAVAILABLE, DPMS_DISABLED, DPMS_ON, DPMS_OFF, DPMS_STANDBY, DPMS_SUSPEND };

    //DPMS extension
    static void UpdateDPMS(bool enable, int seconds);
    static int getDPMSInfo();
    static void WakeUpScreen(bool enable);
};

#endif // XUTILS_H
