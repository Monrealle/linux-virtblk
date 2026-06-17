# Linux Virtual Block Device Module
Разработка виртуального блочного устройства в ядре Linux — учебная практика, 1 курс, Технологии программирования, Математико-механический факультет.

[![CI](https://github.com/Monrealle/linux-virtblk/actions/workflows/lint.yml/badge.svg?style=flat-square)](https://github.com/Monrealle/linux-virtblk/actions/workflows/lint.yml)
[![Docs](https://img.shields.io/badge/docs-github%20pages-blue?style=flat-square)](https://monrealle.github.io/linux-virtblk/)
[![Report](https://img.shields.io/badge/report-pdf-blue?style=flat-square)](docs/report.pdf)
[![Release](https://img.shields.io/badge/release-v1.0-blue?style=flat-square)](https://github.com/Monrealle/linux-virtblk/releases)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange?style=flat-square)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
![Language](https://img.shields.io/badge/language-C-555555?style=flat-square&logo=c&logoColor=white)
![Kernel](https://img.shields.io/badge/kernel-6.18.13--200.fc43.x86__64-555555?style=flat-square&logo=linux&logoColor=white)


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

Модульное тестирование кода ядра в рамках данной практики не применяется: модуль исполняется в kernel space и не запускается в изоляции; специализированные средства (например, KUnit) остаются за пределами задачи. Вместо этого используются интеграционные тесты, работающие с реальным устройством /dev/ram_virtblk:

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

## Быстрая демонстрация
- Загрузка модуля: `./scripts/install.sh`
- Выгрузка модуля: `./scripts/uninstall.sh`

## Анализ кода

Для проверки стиля кода используется [`checkpatch.pl`](scripts/checkpatch/checkpatch.pl) - официальный линтер ядра Linux, запускается автоматически в CI при каждом пуше.

Статические анализаторы типа CodeQL не используются, так как модуль ядра компилируется под конкретную версию ядра (`6.18.13-200.fc43.x86_64`) и не может быть собран в стандартном CI-окружении GitHub.

Стандартные санитайзеры (ASAN/UBSan) неприменимы в kernel space - они работают только в user space. Аналог для ядра (KASAN) требует специальной пересборки ядра с флагом CONFIG_KASAN=y, что выходит за рамки данного проекта. Корректность работы проверяется интеграционными тестами

## Документация

Документация генерируется автоматически из исходного кода через [Doxygen](https://www.doxygen.nl/)
при каждом пуше и публикуется на GitHub Pages. Тема оформления — [Doxygen Awesome](https://github.com/jothepro/doxygen-awesome-css).

[monrealle.github.io/linux-virtblk](https://monrealle.github.io/linux-virtblk/)

## Отчёт

Отчёт по учебной практике: [docs/report.pdf](docs/report.pdf)

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
│   ├── doxygen-awesome-darkmode-toggle.js        - кнопка переключения тёмной/светлой темы
│   ├── doxygen-awesome-fragment-copy-button.js   - копирование блоков кода
│   ├── doxygen-awesome-interactive-toc.js        - подсветка текущего раздела в оглавлении
│   ├── doxygen-awesome-paragraph-link.js         - ссылки на параграфы при наведении на заголовок
│   ├── doxygen-awesome.css                       - основная тема оформления документации
│   ├── footer.html                               - кастомный нижний колонтитул
│   └── header.html                               - кастомный верхний колонтитул страниц документации
│
├── docs/                                         - документация проекта
│   ├── report.pdf                                - отчёт по учебной практике
│   └── report.tex                                - исходник отчёта (LaTeX)
│
├── scripts/                                      - вспомогательные скрипты
│   ├── install.sh                                - сборка и загрузка модуля одной командой
│   ├── uninstall.sh                              - выгрузка модуля
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
