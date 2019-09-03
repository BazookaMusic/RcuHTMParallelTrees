#include <iostream>

void okWithPadding(std::string line) {
    // OK will fit line length
    const int lineLength = 80;

    int padding_amount = 0;
    if ((line.length() - 2) > lineLength) {
        std::cout << std::endl;
        padding_amount = lineLength-3;
    } else {
        padding_amount = (lineLength - 3) - line.length();
    }

    for (; padding_amount > 0; padding_amount--) {
        std::cout << " ";
    }

    std::cout<< "OK" <<std::endl;
}
