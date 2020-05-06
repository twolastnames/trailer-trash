
#include <iostream>

struct Hello {
    void world() {
        std::cout << "hello world" << std::endl;
    }
};

int main() {
    Hello hello;
    hello.world();
}