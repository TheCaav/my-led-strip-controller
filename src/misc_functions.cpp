#include <Arduino.h>
#include <misc_functions.h>

int charToInt (char val) {
    if (val >= 48 && val <= 57) {
        return val - 48; // Numbers
    } else if (val >= 65 && val <= 70) {
        return val - 55; // A = 10
    } else if (val >= 97 && val <= 102) {
        return val - 87; // a = 10
    } else {
        return -1; // ERROR
    }
}

/** 
 * DOES NOT CHECK IF EVERYTHING IS CORRECT!!! 
 */
int hexToInt (String hex) {
    if (hex.substring(0,2) != "0x") {
        return - 1;
    } else if (hex.length() != 8) {
        return - 1;
    }

    char rgb [3][2] = {{hex.charAt(2), hex.charAt(3)}, {hex.charAt(4), hex.charAt(5)}, {hex.charAt(6), hex.charAt(7)}};


    int result = 0;

    // DOES NOT CHECK FOR CORRECT VALUES!!!
    for (int i = 0; i < 3; i++) {
        result += charToInt(rgb[2-i][1]) * pow(16, 2*i);
        result += charToInt(rgb[2-i][0]) * pow(16, 2*i + 1);
    }

    return result;
}

template<typename T> struct map_init_helper
{
    T& data;
    map_init_helper(T& d) : data(d) {}
    map_init_helper& operator() (typename T::key_type const& key, typename T::mapped_type const& value)
    {
        data[key] = value;
        return *this;
    }
};