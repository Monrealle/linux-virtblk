#!/bin/bash
set -e

MODULE=../src/virtblk.ko
DEVICE=/dev/ram_virtblk

echo "===== Загрузка модуля ====="
sudo insmod $MODULE
sleep 1

echo "===== Test 1: базовый write/read ====="
echo "Hello world!" | sudo dd of=$DEVICE bs=512 count=1 2>/dev/null
RESULT=$(sudo dd if=$DEVICE bs=512 count=1 2>/dev/null | head -c 13)
if [ "$RESULT" = "Hello world!" ]; then
    echo "OK: Test 1"
else
    echo "FAIL: got '$RESULT'"
    sudo rmmod virtblk
    exit 1
fi

echo "===== Test 2: файловая система ====="
sudo mkfs.ext4 $DEVICE -q
sudo mkdir -p /mnt/ramtest
sudo mount $DEVICE /mnt/ramtest
echo "test" | sudo tee /mnt/ramtest/hello.txt > /dev/null
RESULT=$(cat /mnt/ramtest/hello.txt)
sudo umount /mnt/ramtest
if [ "$RESULT" = "test" ]; then
    echo "OK: Test 2"
else
    echo "FAIL: got '$RESULT'"
    sudo rmmod virtblk
    exit 1
fi

echo "===== Test 3: целостность данных ====="
sudo dd if=/dev/urandom of=$DEVICE bs=4096 count=128 2>/dev/null
sudo dd if=$DEVICE of=/tmp/snapshot.bin bs=4096 count=128 2>/dev/null
sudo dd if=$DEVICE bs=4096 count=128 2>/dev/null | diff - /tmp/snapshot.bin \
    && echo "OK: Test 3" || { echo "FAIL: data mismatch"; sudo rmmod virtblk; exit 1; }

echo "===== Выгрузка модуля ====="
sudo rmmod virtblk
echo "===== Все тесты пройдены ====="
