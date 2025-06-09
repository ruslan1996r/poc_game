#include <iostream>
#include <unordered_set>
#include <bitset>
#include <chrono>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

const int PORT = 12345;
const int BUFFER_SIZE = 512;
const int WINDOW_SIZE = 5;
const int ACK_TIMEOUT_MS = 500; // Таймаут перед отправкой ACK

struct Packet
{
  uint32_t id;
  char data[BUFFER_SIZE];
};

struct AckPacket
{
  uint32_t firstPacketId;
  std::bitset<WINDOW_SIZE> receivedMask;
};

int main()
{
  int sockfd;
  sockaddr_in serverAddr{}, clientAddr{};
  socklen_t clientLen = sizeof(clientAddr);
  Packet packet{};
  std::unordered_set<uint32_t> receivedIds;
  uint32_t lastAckedId = 0;
  std::chrono::steady_clock::time_point lastPacketTime = std::chrono::steady_clock::now();
  bool newPacketReceived = false; // Флаг, были ли новые пакеты

  // 1. Создаём сокет
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
  {
    std::cerr << "Ошибка при создании сокета" << std::endl;
    return 1;
  }

  // 2. Настраиваем серверный адрес
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(PORT);

  // 3. Привязываем сокет к порту
  if (bind(sockfd, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
  {
    std::cerr << "Ошибка при привязке сокета" << std::endl;
    return 1;
  }

  std::cout << "UDP сервер запущен на порту " << PORT << std::endl;

  while (true)
  {
    // 4. Проверяем, есть ли входящие пакеты
    ssize_t received = recvfrom(sockfd, &packet, sizeof(packet), MSG_DONTWAIT, (sockaddr *)&clientAddr, &clientLen);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPacketTime).count();

    if (received > 0)
    {
      lastPacketTime = now; // Обновляем время последнего пакета
      receivedIds.insert(packet.id);
      std::cout << "Получено [" << packet.id << "]: " << packet.data << std::endl;
      newPacketReceived = true; // Отметим, что новый пакет получен
    }

    // 5. Отправляем Batch ACK, если:
    // - Либо набралось WINDOW_SIZE пакетов
    // - Либо прошло слишком много времени с последнего пакета И были новые пакеты

    // * Отправляет ack либо в случае наполнения ack_batch (WINDOW_SIZE) либо по истечению таймаута
    // * Сделано по причине, что иногда ивенты могут долго не наполнять батч
    if ((receivedIds.size() >= WINDOW_SIZE || elapsed >= ACK_TIMEOUT_MS) && newPacketReceived)
    {
      AckPacket ack{};
      ack.firstPacketId = lastAckedId + 1;

      for (int i = 0; i < WINDOW_SIZE; i++)
      {
        if (receivedIds.count(ack.firstPacketId + i))
        {
          ack.receivedMask.set(i); // Заполняет маску, что-то вроде -> 0 1 1 0 1 1 1
        }
      }

      sendto(sockfd, &ack, sizeof(ack), 0, (sockaddr *)&clientAddr, clientLen);
      std::cout << "Отправлен Batch ACK (ID " << ack.firstPacketId << ")" << std::endl;

      lastAckedId += WINDOW_SIZE;
      receivedIds.clear();
      lastPacketTime = now;      // Сбрасываем таймер
      newPacketReceived = false; // Сбрасываем флаг
    }
  }

  close(sockfd);
  return 0;
}
