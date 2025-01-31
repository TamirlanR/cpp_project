CXX = g++
CXXFLAGS = -std=c++17 -pthread -I/usr/include/nlohmann
LDFLAGS = -lpcap

SRC_DIR = src
BIN_DIR = bin

SRCS = $(SRC_DIR)/main.cpp $(SRC_DIR)/capture.cpp $(SRC_DIR)/base64.cpp
TARGET = $(BIN_DIR)/monitor

all: $(TARGET)

$(TARGET): $(SRCS)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

run: $(TARGET)
	cp traffic_log.json my-app/public/  # Копируем логи в React
	cd my-app && npm run dev & echo $$! > ../react.pid  # Запускаем React и сохраняем PID
	./bin/monitor start                 # Запускаем мониторинг

stop:
	./bin/monitor stop                  # Останавливаем мониторинг
	if [ -f react.pid ]; then kill `cat react.pid` && rm react.pid; fi  # Останавливаем React

clean:
	rm -rf $(BIN_DIR)/*.o $(TARGET) react.pid
