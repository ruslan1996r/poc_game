#include <iostream>
#include <unordered_set>
#include <bitset>
#include <chrono>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <thread>

const int PORT = 12345;
const int BUFFER_SIZE = 512;
const int WINDOW_SIZE = 5;
const int ACK_TIMEOUT_MS = 100;
const int TICK_RATE = 60;
const int TICK_DURATION_MS = 1000 / TICK_RATE;

struct Packet {
  uint32_t id;
  char data[BUFFER_SIZE];
};

struct AckPacket {
  uint32_t firstPacketId;
  std::bitset<WINDOW_SIZE> receivedMask;
};

int main() {
  int sockfd;
  sockaddr_in serverAddr{}, clientAddr{};
  socklen_t clientLen = sizeof(clientAddr);
  Packet packet{};
  std::unordered_set<uint32_t> receivedIds;
  uint32_t lastAckedId = 0;
  bool newPacketReceived = false;
  auto lastPacketTime = std::chrono::steady_clock::now();

  // Создание сокета
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    std::cerr << "Ошибка при создании сокета" << std::endl;
    return 1;
  }

  // Привязка
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(PORT);
  if (bind(sockfd, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
    std::cerr << "Ошибка при привязке сокета" << std::endl;
    return 1;
  }

  std::cout << "UDP сервер запущен на порту " << PORT << std::endl;

  // Настройка poll
  pollfd pfd{};
  pfd.fd = sockfd;
  pfd.events = POLLIN;

  while (true) {
    auto tickStart = std::chrono::steady_clock::now();

    // Проверяем входящие данные (не блокируя надолго)
    int pollResult = poll(&pfd, 1, 0); // 0 — моментальный возврат

    if (pollResult > 0 && (pfd.revents & POLLIN)) {
      ssize_t received = recvfrom(sockfd, &packet, sizeof(packet), 0, (sockaddr *)&clientAddr, &clientLen);
      if (received > 0) {
        receivedIds.insert(packet.id);
        std::cout << "Получено [" << packet.id << "]: " << packet.data << std::endl;
        newPacketReceived = true;
        lastPacketTime = tickStart;
      }
    }

    // Отправка Batch ACK
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPacketTime).count();
    if ((receivedIds.size() >= WINDOW_SIZE || elapsed >= ACK_TIMEOUT_MS) && newPacketReceived) {
      AckPacket ack{};
      ack.firstPacketId = lastAckedId + 1;

      for (int i = 0; i < WINDOW_SIZE; i++) {
        if (receivedIds.count(ack.firstPacketId + i)) {
          ack.receivedMask.set(i);
        }
      }

      sendto(sockfd, &ack, sizeof(ack), 0, (sockaddr *)&clientAddr, clientLen);
      std::cout << "Отправлен Batch ACK (ID " << ack.firstPacketId << ")" << std::endl;

      lastAckedId += WINDOW_SIZE;
      receivedIds.clear();
      lastPacketTime = now;
      newPacketReceived = false;
    }

    // 💤 Ждём до конца тика (поддерживаем 60 FPS)
    auto tickEnd = std::chrono::steady_clock::now();
    auto tickElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tickEnd - tickStart).count();
    if (tickElapsed < TICK_DURATION_MS) {
      std::this_thread::sleep_for(std::chrono::milliseconds(TICK_DURATION_MS - tickElapsed));
    }
  }

  close(sockfd);
  return 0;
}
