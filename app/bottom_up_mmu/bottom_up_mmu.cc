#include <utility/ostream.h>

using namespace EPOS;

OStream cout;

int main()
{
    cout << "Hello, world!" << endl;

    int * a =  new int;
    *a = 1;
    cout << "Address a = " << a << endl;
    cout << " a = " << *a << endl;


    int * b = new int;
    *b = 2;
    cout << "Address b = " << b << endl;
    cout << " b = " << *b << endl;


    int * c = new int;
    *c = 3;
    cout << "Address c = " << c << endl;
    cout << "c = " << *c << endl;


    int * d = new int;
    *d = 4;
    cout << "Address d = " << d << endl;
    cout << "d = " << *d << endl;


    // Verifies if they are properly ordered.
    // assert(a < b);
    // assert(b < c);
    // assert(c < d);

    free(b);
    cout << "Freeing b: " << *b << endl;

    free(c);
    cout << "Freeing c: " << *c << endl;

    int * f = new int;
    *f = 5;
    cout << "Address f = " << f << endl;
    cout << "f = " << *f << endl;


    int * g = new int;
    *g = 6;
    cout << "Address g = " << g << endl;
    cout << "g = " << *g << endl;


    // Verifies if f and g are at the same addresses of b and c.
    // assert(f == b);
    // assert(g == c);

    return 0;
}
