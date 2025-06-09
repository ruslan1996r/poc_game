#include <iostream>
#include <unordered_map>
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
const int TICK_RATE = 5;
const int TICK_DURATION_MS = 1000 / TICK_RATE;

struct Packet {
  uint32_t sessionId;
  uint32_t id;
  char data[BUFFER_SIZE];
};

struct AckPacket {
  uint32_t sessionId;
  uint32_t firstPacketId;
  std::bitset<WINDOW_SIZE> receivedMask;
};

int main() {
  int sockfd;
  sockaddr_in serverAddr{}, clientAddr{};
  socklen_t clientLen = sizeof(clientAddr);
  Packet packet{};

  std::unordered_map<uint32_t, std::unordered_set<uint32_t>> receivedIds;
  std::unordered_map<uint32_t, uint32_t> lastAckedId;
  std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> lastPacketTime;
  std::unordered_map<uint32_t, bool> newPacketReceived;

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    std::cerr << "Ошибка при создании сокета" << std::endl;
    return 1;
  }

  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(PORT);
  if (bind(sockfd, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
    std::cerr << "Ошибка при привязке сокета" << std::endl;
    return 1;
  }

  std::cout << "UDP сервер запущен на порту " << PORT << std::endl;

  pollfd pfd{};
  pfd.fd = sockfd;
  pfd.events = POLLIN;

  while (true) {
    auto tickStart = std::chrono::steady_clock::now();

    int pollResult = poll(&pfd, 1, 0);
    if (pollResult > 0 && (pfd.revents & POLLIN)) {
      ssize_t received = recvfrom(sockfd, &packet, sizeof(packet), 0, (sockaddr *)&clientAddr, &clientLen);
      if (received > 0) {
        uint32_t sid = packet.sessionId;
        receivedIds[sid].insert(packet.id);
        if (lastAckedId.find(sid) == lastAckedId.end()) {
          lastAckedId[sid] = 0;
        }

        lastPacketTime[sid] = tickStart;
        newPacketReceived[sid] = true;

        std::cout << "Получено [SID " << sid << ", " << packet.id << "]: " << packet.data << std::endl;
      }
    }

    auto now = std::chrono::steady_clock::now();
    for (auto &[sid, ids] : receivedIds) {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPacketTime[sid]).count();

      if ((ids.size() >= WINDOW_SIZE || elapsed >= ACK_TIMEOUT_MS) && newPacketReceived[sid]) {
        uint32_t firstId = lastAckedId[sid] + 1;

        AckPacket ack{};
        ack.sessionId = sid;
        ack.firstPacketId = firstId;

        for (int i = 0; i < WINDOW_SIZE; i++) {
          if (receivedIds[sid].count(firstId + i)) {
            ack.receivedMask.set(i);
          }
        }

        sendto(sockfd, &ack, sizeof(ack), 0, (sockaddr *)&clientAddr, clientLen);
        std::cout << "Отправлен Batch ACK (Session: " << sid << ", ID " << ack.firstPacketId
                  << ", mask: " << ack.receivedMask << ")" << std::endl;

        // Продвигаем lastAckedId только по подряд подтверждённым пакетам
        uint32_t newLastAckedId = lastAckedId[sid];
        for (int i = 0; i < WINDOW_SIZE; i++) {
          if (ack.receivedMask[i]) {
            newLastAckedId = firstId + i;
          } else {
            break;
          }
        }
        lastAckedId[sid] = newLastAckedId;

        // Удаляем только подтверждённые ID из receivedIds
        for (int i = 0; i < WINDOW_SIZE; i++) {
          if (ack.receivedMask[i]) {
            receivedIds[sid].erase(firstId + i);
          }
        }

        lastPacketTime[sid] = now;
        newPacketReceived[sid] = false;
      }
    }

    auto tickEnd = std::chrono::steady_clock::now();
    auto tickElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tickEnd - tickStart).count();
    if (tickElapsed < TICK_DURATION_MS) {
      std::this_thread::sleep_for(std::chrono::milliseconds(TICK_DURATION_MS - tickElapsed));
    }
  }

  close(sockfd);
  return 0;
}
