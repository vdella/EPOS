#include <utility/ostream.h>
#include <memory.h>

using namespace EPOS;

OStream cout;

#ifdef __cortex_m__
const unsigned ES1_SIZE = 10000;
const unsigned ES2_SIZE = 100000;
#else
const unsigned ES1_SIZE = 100;
const unsigned ES2_SIZE = 200;
#endif

int main()
{
    OStream cout;

    cout << "Hello World!" << endl;

    return 0;
}
