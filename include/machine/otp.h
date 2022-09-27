// EPOS OTP Mediator Common Package

#ifndef __otp_h
#define __otp_h

#include <system/config.h>

__BEGIN_SYS

class OTP_Common
{
protected:
    OTP_Common() {}

public:
    int read_shot(int offset, void *buf, int size);
    int write_shot(int offset, const void *buf, int size);
};

__END_SYS

#endif

#if defined(__OTP_H) && !defined(__otp_common_only__)
#include __OTP_H
#endif
