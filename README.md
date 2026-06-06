# Linux Virtual Block Device Module
Разработка виртуального блочного устройства в ядре Linux — учебная практика, 1 курс, Технологии программирования, Математико-механический факультет.

![CI](https://github.com/Monrealle/linux-virtblk/actions/workflows/lint.yml/badge.svg)
[![Docs](https://img.shields.io/badge/docs-github%20pages-blue)](https://monrealle.github.io/linux-virtblk/)
![License](https://img.shields.io/github/license/Monrealle/linux-virtblk)

## Описание
Модуль ядра Linux, реализующий виртуальное блочное устройство `/dev/ram_virtblk`, которое хранит данные в оперативной памяти. Устройство ведёт себя как обычный диск: поддерживает форматирование, монтирование и файловые операции — данные при этом хранятся в RAM и теряются при выгрузке модуля.

**Параметры устройства:**
- Размер: 64 MiB (131072 секторов по 512 байт)
- Тип: BIO-based, без request queue и blk-mq
- Устройство: `/dev/ram_virtblk`

## Окружение

- ОС: Fedora Server 43
- Ядро: `6.18.13-200.fc43.x86_64`
- Гипервизор: VirtualBox

## Зависимости

```bash
sudo dnf install gcc make kernel-devel kernel-headers
```

## Сборка

```bash
git clone https://github.com/Monrealle/linux-virtblk
cd linux-virtblk
make -C src
```

## Использование

```bash
# Загрузить модуль
sudo insmod src/virtblk.ko

# Проверить что устройство появилось
lsblk | grep ram_virtblk

# Выгрузить модуль
sudo rmmod virtblk
```

## Тестирование

Тесты привязаны к конкретному ядру (`6.18.13-200.fc43.x86_64`) и запускаются автоматически в CI через self-hosted runner на целевой машине при каждом пуше. Если машина выключена — CI ждёт пока runner появится онлайн.

Для запуска тестов вручную:

```bash
chmod +x test/test.sh
cd test && bash test.sh
```

Тесты проверяют:
- **Базовый write/read** — запись строки и чтение обратно
- **Файловая система** — монтирование ext4, запись и чтение файла
- **Целостность данных** — запись случайных данных и побайтовое сравнение снапшота
- **Граница устройства** — проверка обработки запроса за пределами устройства

## Документация

Документация генерируется автоматически из исходного кода через [Doxygen](https://www.doxygen.nl/)
при каждом пуше и публикуется на GitHub Pages. Тема оформления — [Doxygen Awesome](https://github.com/jothepro/doxygen-awesome-css).

[monrealle.github.io/linux-virtblk](https://monrealle.github.io/linux-virtblk/)

## Структура проекта

```
linux-virtblk
├── .github/                                      - конфигурация GitHub
│   ├── CODEOWNERS                                - владельцы кода
│   └── workflows/                                - CI/CD пайплайны
│       ├── lint.yml                              - линтинг и генерация документации (CI)
│       └── test.yml                              - запуск тестов (self-hosted runner)
│
├── assets/                                       - статические ресурсы для Doxygen
│   ├── custom.css                                - пользовательские переопределения стилей
│   ├── doxygen-awesome-darkmode-toggle.js        - кнопка переключения тёмной/светлой темы
│   ├── doxygen-awesome-fragment-copy-button.js   - копирование блоков кода
│   ├── doxygen-awesome-interactive-toc.js        - подсветка текущего раздела в оглавлении
│   ├── doxygen-awesome-paragraph-link.js         - ссылки на параграфы при наведении на заголовок
│   ├── doxygen-awesome.css                       - основная тема оформления документации
│   ├── footer.html                               - кастомный нижний колонтитул
│   ├── header.html                               - кастомный верхний колонтитул страниц документации
│   └── doxygen-awesome.css                       - тема оформления документации
│
├── scripts/                                      - вспомогательные скрипты
│   └── checkpatch/                               - проверка стиля кода ядра Linux
│       ├── checkpatch.pl                         - скрипт проверки стиля кода
│       ├── spelling.txt                          - словарь опечаток
│       └── const_structs.checkpatch              - список константных структур
│
├── src/                                          - исходный код модуля
│   ├── virtblk.c                                 - исходный код модуля
│   ├── virtblk.h                                 - константы и параметры устройства
│   └── Makefile                                  - сборка модуля
│
├── test/                                         - тесты устройства
│   └── test.sh                                   - интеграционные тесты
│
├── .gitignore                                    - игнорируемые файлы
├── Doxyfile                                      - конфигурация генератора документации
├── LICENSE                                       - лицензия
└── README.md                                     - документация проекта
```
