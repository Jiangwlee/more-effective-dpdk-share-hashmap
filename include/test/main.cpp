#include <iostream>
#include "common.h"

using namespace std;

int main(void) {
    unsigned int a = 1 << 10;
    unsigned int b = (1 << 30) - 1;

    if (is_power_of_2(a))
        cout << a << " is power of 2" << endl;
    else {
        cout << a << " is not power of 2" << endl;
        int c = convert_to_power_of_2(a);
        cout << "Convert " << a << " to power of 2 : " << c << endl;
    }

    if (is_power_of_2(b))
        cout << b << " is power of 2" << endl;
    else {
        cout << b << " is not power of 2" << endl;
        int c = convert_to_power_of_2(b);
        cout << "Convert " << b << " to power of 2 : " << c << endl;
    }

    return 0;
}
