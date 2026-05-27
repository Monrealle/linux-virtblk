# Linux Virtual Block Device Module
Разработка виртуального блочного устройства в ядре Linux — учебная практика, 1 курс, Технологии программирования, Математико-механический факультет.

![CI](https://github.com/Monrealle/linux-virtblk/actions/workflows/lint.yml/badge.svg)

## Описание
Модуль ядра Linux, реализующий виртуальное блочное устройство `/dev/ram_virtblk`, которое хранит данные в оперативной памяти. Устройство ведёт себя как обычный диск: поддерживает форматирование, монтирование и файловые операции — данные при этом хранятся в RAM и теряются при выгрузке модуля.

**Параметры устройства:**
- Размер: 1 MiB (2048 секторов по 512 байт)
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
cd linux-virtblk/src
make
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

## Структура проекта

```
linux-virtblk
├── .github/
│   ├── CODEOWNERS                     - владельцы кода
│   └── workflows/
│       ├── lint.yml                   - сборка и проверка стиля кода (CI)
│       └── test.yml                   - запуск тестов (self-hosted runner)
├── scripts/
│   └── checkpatch/
│       ├── checkpatch.pl              - скрипт проверки стиля кода
│       ├── spelling.txt               - словарь опечаток
│       └── const_structs.checkpatch   - список константных структур
├── src/
│   ├── virtblk.c                      - исходный код модуля
│   └── Makefile                       - сборка модуля
├── test/
│   └── test.sh                        - тесты устройства
├── .gitignore                         - игнорируемые файлы
├── LICENSE                            - лицензия
└── README.md                          - документация проекта
```
