#!/bin/bash
set -e

echo "Сборка модуля..."
make -C ../src

echo "Загрузка модуля virtblk.ko..."
sudo insmod ../src/virtblk.ko

echo "Модуль успешно загружен."
echo "Проверить можно: lsmod | grep virtblk"
