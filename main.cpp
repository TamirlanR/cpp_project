#include <iostream>
#include <thread>
#include <cstdlib>
#include <atomic>
#include "capture.h"

std::atomic<bool> isRunning(false);

void startCapture() {
    if (!isRunning) {
        std::cout << "Запуск захвата трафика..." << std::endl;
        isRunning = true;
        startPacketCapture();  // Запуск анализа пакетов
        isRunning = false;
    }
}

void stopCapture() {
    if (isRunning) {
        std::cout << "Остановка захвата трафика..." << std::endl;
        std::system("pkill monitor");  // Остановка процесса
        isRunning = false;
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string command = argv[1];

        if (command == "start") {
            std::thread captureThread(startCapture);
            captureThread.detach();
            std::cout << "Мониторинг запущен." << std::endl;
        } else if (command == "stop") {
            stopCapture();
            std::cout << "Мониторинг остановлен." << std::endl;
        } else {
            std::cerr << "Неверная команда. Используйте: ./monitor [start|stop]" << std::endl;
        }
    } else {
        std::cout << "Использование: ./monitor [start|stop]" << std::endl;
    }

    return 0;
}
