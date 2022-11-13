#include <utility/ostream.h>
#include <architecture.h>

using namespace EPOS;

typedef CPU::Phy_Addr Phy_Addr;

OStream cout;

int main()
{
    cout << "Hello, world!" << endl;
    //
    // int * b = new int;
    //
    // cout << "b addr: " << b << endl;
    //
    // unsigned int * a = new(b) unsigned int[6 * 1024*1024];
    //
    // cout << "a addr: " << a << endl;
    //
    //
    // memset(a, 10, 6);
    //
    // Phy_Addr addr = reinterpret_cast<unsigned long>((void*)0x80000000);
    // addr += 6 * 1024 * 1024;
    //
    // cout << "addr: " << addr << " value " << *(reinterpret_cast<unsigned long*>((void*)addr)) << endl;
    //
    //
    //
    // cout << "a addr: " << a << endl;
    //
    //
    // // int * a = new int;
    // cout << "a -> " << a << endl;
    //
    // *a = 9;
    //
    // int * b = new int;
    // cout << "b -> " << b << endl;
    //
    // *b = 10;
    //
    // Phy_Addr addr = reinterpret_cast<unsigned long>(a);
    // Phy_Addr addr2 = addr - 32;
    //
    // cout << "address = " << addr << endl;
    //
    // addr += 1;
    // cout << "address = " << addr << " value = " << *(reinterpret_cast<unsigned long*>((void*)addr)) << endl;
    //
    // addr += 8;
    // cout << "address = " << addr << " value = " << *(reinterpret_cast<unsigned long*>((void*)addr)) << endl;
    //
    // addr += 30;
    // cout << "address = " << addr2 << " value = " << *(reinterpret_cast<unsigned long*>((void*)addr2)) << endl;


    // char sus[10] = "mto sus";
    //
    // cout << "Address SUS = " << &sus << endl;
    //
    //
    // cout << "1: " << &(sus[0]) << endl;
    // cout << "2: " << &(sus[1]) << endl;
    // cout << "3: " << &(sus[2]) << endl;
    // cout << "4: " << &(sus[3]) << endl;
    //
    // cout << "4: " << &sus + 1 << endl;
    //
    //
    // cout << "1: " << &sus << endl;
    // cout << "2: " << reinterpret_cast<char*>(0x000000008010ff09) << endl;
    // cout << "3: " << reinterpret_cast<char*>(0x000000008010ff0a) << endl;
    // cout << "4: " << reinterpret_cast<char*>(0x000000008010ff0b) << endl;
    //
    // unsigned int z;
    // cout << "Address z = " << &z << endl;
    //
    // unsigned int * a =  new unsigned int;
    // *a = 1;
    // cout << "Address a = " << a << endl;
    // cout << " a = " << *a << endl;
    //
    // unsigned long * p = new unsigned long;
    //
    //
    // cout << "Pointer Size: " << sizeof(p) << endl;
    //
    //
    // unsigned int * b = new unsigned int;
    // *b = 2;
    // cout << "Address b = " << b << endl;
    // cout << " b = " << *b << endl;
    //
    //
    // unsigned int * c = new unsigned int;
    // // *c = 3;
    // cout << "Address c = " << c << endl;
    // cout << "c = " << *c << endl;
    //
    //
    // unsigned int * d = new unsigned int;
    // *d = 4;
    // cout << "Address d = " << d << endl;
    // cout << "d = " << *d << endl;
    //
    //
    // // Verifies if they are properly ordered.
    // // assert(a < b);
    // // assert(b < c);
    // // assert(c < d);
    //
    // free(b);
    // cout << "Freeing b: " << *b << endl;
    //
    // free(c);
    // cout << "Freeing c: " << *c << endl;
    //
    // unsigned int * f = new unsigned int;
    // *f = 5;
    // cout << "Address f = " << f << endl;
    // cout << "f = " << *f << endl;
    //
    //
    // unsigned int * g = new unsigned int;
    // *g = 6;
    // cout << "Address g = " << g << endl;
    // cout << "g = " << *g << endl;
    //
    // unsigned int h;
    // cout << "Address h " << &h << endl;
    //
    // unsigned int i;
    // cout << "Address i " << &i << endl;
    //
    // unsigned int j;
    // cout << "Address j " << &j << endl;
    //
    // unsigned long k;
    // cout << "Address k " << &k << endl;
    //
    // unsigned long l;
    // cout << "Address l " << &l << endl;
    //
    // unsigned long m;
    // cout << "Address m " << &m << endl;
    //
    // unsigned long n;
    // cout << "Address n " << &n << endl;
    //
    //
    // //
    // unsigned int test[1024 * 1024];
    // //
    // cout << "test addr " << &test << endl;


    // Verifies if f and g are at the same addresses of b and c.
    // assert(f == b);
    // assert(g == c);

    return 0;
}
