#include <utility/ostream.h>

using namespace EPOS;

OStream cout;

int main()
{
    cout << "Hello, world!" << endl;

    int * a =  new int;
    cout << "Address a = " << a << endl;

    int * b = new int;
    cout << "Address b = " << b << endl;

    int * c = new int;
    cout << "Address c = " << c << endl;

    int * d = new int;
    cout << "Address d = " << d << endl;

    int * e = new int;
    cout << "Address e = " << e << endl;

    assert(a < b);
    assert(b < c);
    assert(c < d);
    assert(d < e);

    cout << "Freeing b." << endl;
    free(b);

    cout << "Freeing c." << endl;
    free(c);

    cout << "Freeing d." << endl;
    free(d);

    int * f = new int;
    cout << "Address f = " << f << endl;

    int * g = new int;
    cout << "Address g = " << g << endl;

    return 0;
}