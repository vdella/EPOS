// EPOS Scheduler Test Program

#include <time.h>
#include <synchronizer.h>
#include <process.h>

using namespace EPOS;

const int iterations = 10;

Mutex table;

Thread * phil[5];
Semaphore * chopstick[5];

OStream cout;

int philosopher(int n, int l, int c);
void think(unsigned long long n);
void eat(unsigned long long n);
unsigned long long busy_wait(unsigned long long n);

int main()
{
    table.lock();
    cout << "The Philosopher's Dinner:" << endl;

    for(int i = 0; i < 5; i++)
        chopstick[i] = new Semaphore;

    phil[0] = new Thread(&philosopher, 0,  5, 32);
    phil[1] = new Thread(&philosopher, 1, 10, 44);
    phil[2] = new Thread(&philosopher, 2, 16, 39);
    phil[3] = new Thread(&philosopher, 3, 16, 24);
    phil[4] = new Thread(&philosopher, 4, 10, 20);

    cout << "Philosophers are alive and hungry!" << endl;

    cout << "The dinner is served ..." << endl;
    table.unlock();

    for(int i = 0; i < 5; i++) {
        int ret = phil[i]->join();
        table.lock();
        cout << "Philosopher " << i << " ate " << ret << " times " << endl;
        table.unlock();
    }

    for(int i = 0; i < 5; i++)
        delete chopstick[i];
    for(int i = 0; i < 5; i++)
        delete phil[i];

    cout << "The end!" << endl;

    return 0;
}

int philosopher(int n, int l, int c)
{
    int first = (n < 4)? n : 0;
    int second = (n < 4)? n + 1 : 4;

    for(int i = iterations; i > 0; i--) {

        table.lock();
        cout << n << ": thinking" << endl;
        table.unlock();

        think(1000000);

        table.lock();
        cout << n << ": hungry" << endl;
        table.unlock();

        chopstick[first]->p();   // get first chopstick
        chopstick[second]->p();  // get second chopstick

        table.lock();
        cout << n << ": eating" << endl;
        table.unlock();

        eat(500000);

        table.lock();
        cout << n << ": sate" << endl;
        table.unlock();

        chopstick[first]->v();   // release first chopstick
        chopstick[second]->v();  // release second chopstick
    }

    table.lock();
    cout << "  done  ";
    table.unlock();

    return iterations;
}

void eat(unsigned long long n) {
    busy_wait(n);
}

void think(unsigned long long n) {
    busy_wait(n);
}

unsigned long long busy_wait(unsigned long long n)
{
    volatile unsigned long long v;
    for(unsigned long long int j = 0; j < 20 * n; j++)
        v &= 2 ^ j;
    return v;
}
