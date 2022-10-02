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

    // Verifies if they are properly ordered.
    assert(a < b);
    assert(b < c);
    assert(c < d);

    cout << "Freeing b." << endl;
    free(b);

    cout << "Freeing c." << endl;
    free(c);

    int * f = new int;
    cout << "Address f = " << f << endl;

    int * g = new int;
    cout << "Address g = " << g << endl;

    // Verifies if f and g are at the same addresses of b and c.
    assert(f == b);
    assert(g == c);

    return 0;
}