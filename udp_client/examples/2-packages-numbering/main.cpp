#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

const int SERVER_PORT = 12345;
const char *SERVER_IP = "127.0.0.1";
const int BUFFER_SIZE = 512;
const int MAX_RETRIES = 3;
const int TIMEOUT_MS = 1000;

struct Packet
{
  uint32_t id;
  char data[BUFFER_SIZE];
};

int main()
{
  int sockfd;
  sockaddr_in serverAddr{};
  socklen_t serverLen = sizeof(serverAddr);
  Packet packet{};
  uint32_t ackId;

  // 1. Создаём UDP-сокет
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
  {
    std::cerr << "Ошибка при создании сокета" << std::endl;
    return 1;
  }

  // 2. Настраиваем серверный адрес
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(SERVER_PORT);
  inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr); // Преобразует строку в числовой IP-адрес

  // 3. Устанавливаем таймаут для ожидания ACK
  struct timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec = TIMEOUT_MS * 1000;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  // 4. Цикл отправки нескольких сообщений
  for (uint32_t packetId = 1; packetId <= 5; packetId++)
  {
    packet.id = packetId;
    snprintf(packet.data, BUFFER_SIZE, "Сообщение #%u", packet.id); // Форматирует строку и записывает её в буфер

    for (int attempt = 0; attempt < MAX_RETRIES; attempt++)
    {
      // 5. Отправляем пакет серверу
      sendto(sockfd, &packet, sizeof(packet), 0, (sockaddr *)&serverAddr, serverLen);
      std::cout << "Отправлено [" << packet.id << "]: " << packet.data << " (попытка " << attempt + 1 << ")" << std::endl;

      // 6. Ждём ACK
      ssize_t received = recvfrom(sockfd, &ackId, sizeof(ackId), 0, (sockaddr *)&serverAddr, &serverLen);

      if (received > 0 && ackId == packet.id)
      {
        std::cout << "Получен ACK для пакета [" << ackId << "]" << std::endl;
        break; // Если получили ACK, идём к следующему пакету
      }
      else
      {
        std::cout << "ACK не получен, повторная отправка..." << std::endl;
      }
    }

    sleep(1); // Имитация паузы перед следующим сообщением
  }

  close(sockfd);
  return 0;
}
