#include <iostream>
#include "timer.h"

int main() {
    Timer timer;
    timer.start(1000, []() {
        std::cout << "Timer expired!" << std::endl;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    timer.stop();

    timer.start(1000, []() {
        std::cout << "Timer expired!" << std::endl;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    timer.start(1000, []() {
        std::cout << "Timer expired!" << std::endl;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    timer.stop();

    return 0;
}
