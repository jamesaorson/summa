#include <template_cpp/identity.hpp>

#include <iostream>

int main() {
    template_cpp::identity id;
    std::cout << "identity(42) = " << id(42) << "\n";
    return 0;
}
