# sandbox_task_int3
## Описание

Проект состоит из многопоточного TCP-сервера и однопоточного консольного клиента. Клиент отправляет запросы в формате JSON серверу, который обрабатывает команды `CheckLocalFile` и `QuarantineLocalFile`.

## Требования
- CMake 3.10 или выше
- Компилятор с поддержкой C++17
- Windows

## Структура JSON-запроса

Запросы передаются в формате JSON и имеют следующий вид:

```json
{
    "command": "CheckLocalFile",
    "params": {
        "filePath": "path/to/file",
        "signature": "68656c6c6f"
    }
}

{
    "command": "QuarantineLocalFile",
    "params": {
        "filePath": "path/to/file"
    }
}
```

## Инструкции по сборке
1. Клонируйте репозиторий и перейдите в каталог с проектом::

  ```bash
    git clone https://github.com/Smoktyn/sandbox_task_int3.git
    cd sandbox_task_int3
  ```

2. Создайте и перейдите в директорию для сборки:

  ```bash
  mkdir build
  cd build
  ```

3. Запустите CMake и соберите проект:

  ```bash
  cmake ..
  cmake --build . --config Release
  ```

## Запуск сервера
Сервер принимает два аргумента командной строки — каталог для карантина и необязательный аргумент — количество потоков в пуле обработки запросов. Например:

  ```bash
  server.exe C:/quarantine
  server.exe C:/quarantine 6
  ```

## Запуск клиента
Клиент принимает команду и параметры или JSON-запрос. Например:

```bash
client.exe CheckLocalFile C:\test.exe 4d5a
client.exe QuarantineLocalFile C:\test.exe
client.exe "{\"command\":\"CheckLocalFile\", \"params\":{\"filePath\":\"C:\\test.exe\", \"signature\":\"4d5a\"}}"
client.exe "{\"command\":\"QuarantineLocalFile\", \"params\":{\"filePath\":\"C:\\test.exe\"}}"
```

