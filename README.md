# Linux Virtual Block Device Module
Разработка виртуального блочного устройства в ядре Linux — учебная практика, 1 курс, Технологии программирования, Математико-механический факультет.

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

## Тестирование

### Базовый тест — запись и чтение

```bash
echo "Hello world!" | sudo dd of=/dev/ram_virtblk bs=512 count=1
sudo dd if=/dev/ram_virtblk bs=512 count=1 | head -c 30
```

### Тест с файловой системой

```bash
sudo mkfs.ext4 /dev/ram_virtblk
sudo mkdir -p /mnt/ramtest
sudo mount /dev/ram_virtblk /mnt/ramtest

echo "test" | sudo tee /mnt/ramtest/hello.txt
cat /mnt/ramtest/hello.txt

sudo umount /mnt/ramtest
```

### Тест целостности данных

```bash
# Записываем случайные данные
sudo dd if=/dev/urandom of=/dev/ram_virtblk bs=4096 count=128 status=progress

# Сохраняем снапшот
sudo dd if=/dev/ram_virtblk of=/tmp/snapshot.bin bs=4096 count=128

# Сравниваем — вывод должен быть пустым (0 байт разницы)
sudo dd if=/dev/ram_virtblk bs=4096 count=128 | diff - /tmp/snapshot.bin && echo "OK: data matches"
```

## Структура проекта

```
.
├── src/
│   ├── virtblk.c       - исходный код модуля
│   └── Makefile        - сборка модуля
├── .gitignore          - игнорируемые файлы
├── LICENSE             - лицензия
└── README.md           - этот файл

```
