#include <utility/ostream.h>
#include <machine/otp.h>

using namespace EPOS;

OStream cout;
SiFive_OTP otp;

const unsigned int BUF_SIZE = 1620;  // As a multiple of 4.
const unsigned int offset = 0x00;

int main()
{
    unsigned int write_buffer[BUF_SIZE];
    unsigned int read_buffer[BUF_SIZE]; // 4 bytes buffer * buf_size

    for(unsigned int i = 0; i < BUF_SIZE; i++)
    {
        write_buffer[i] = 3;
        read_buffer[i] = 10;
    }

    unsigned int b = (unsigned int) otp.write_shot(offset, &write_buffer, BUF_SIZE);
    // 0 * 4, write_buffer, (3840 - 0) * 4

    cout << "write size = " << b << endl;

    for (unsigned int i = 0; i < b; i++)
    {
        cout << hex << "> buf=" << write_buffer[i] << endl;
    }

    unsigned int c = (unsigned int) otp.read_shot(offset, &read_buffer, BUF_SIZE);

    cout << "read size = " << c << endl;

    for (unsigned int i = 0; i < c; i++)
    {
        cout << hex << "> buf=" << read_buffer[i] << endl;
    }

    return 0;
}