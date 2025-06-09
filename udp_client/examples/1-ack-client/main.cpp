#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

const int SERVER_PORT = 12345;
const char *SERVER_IP = "127.0.0.1";
const int BUFFER_SIZE = 1024;
const int MAX_RETRIES = 3;   // Количество повторных попыток
const int TIMEOUT_MS = 1000; // Время ожидания ACK (1 секунда)

int main()
{
  int sockfd;
  sockaddr_in serverAddr{};
  char buffer[BUFFER_SIZE];

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

  // 3. Настраиваем таймаут для ожидания ACK
  struct timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec = TIMEOUT_MS * 1000;
  // Заставляет сервер ждать ACK по таймауту
  // Если ACK не придёт за 1 секунду, recvfrom() вернёт ошибку, и клиент отправит данные заново.
  // Когда я вызову recv() или recvfrom(), жди максимум X миллисекунд. Если данных за это время не придёт — верни ошибку (timeout).
  // В случае ошибки received = -1
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  std::string message = "Привет, сервер!";
  socklen_t serverLen = sizeof(serverAddr);

  for (int attempt = 0; attempt < MAX_RETRIES; attempt++)
  {
    // 4. Отправляем сообщение серверу
    sendto(sockfd, message.c_str(), message.length(), 0, (sockaddr *)&serverAddr, serverLen);
    std::cout << "Отправлено: " << message << " (попытка " << attempt + 1 << ")" << std::endl;

    // 5. Ожидаем ACK
    ssize_t received = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (sockaddr *)&serverAddr, &serverLen);

    if (received > 0)
    {
      buffer[received] = '\0';
      std::cout << "Получен ACK: " << buffer << std::endl;
      break; // ACK получен, выходим из цикла
    }
    else
    {
      std::cout << "ACK не получен, повторная отправка..." << std::endl;
    }
  }

  close(sockfd);
  return 0;
}
