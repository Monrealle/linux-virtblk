#!/bin/bash
set -eo pipefail
MODULE=../src/virtblk.ko
DEVICE=/dev/ram_virtblk

cleanup() {
    echo "===== Выгрузка модуля ====="
    sudo umount /mnt/ramtest 2>/dev/null || true
    sudo rmmod virtblk 2>/dev/null || true
    sudo rm -f /tmp/snapshot.bin || true
}
trap cleanup EXIT

echo "===== Загрузка модуля ====="
sudo insmod "$MODULE"
sleep 1

echo "===== Test 1: базовый write/read ====="
echo -n "Hello world!" | sudo dd of="$DEVICE" bs=512 count=1 2>/dev/null
RESULT=$(sudo dd if="$DEVICE" bs=512 count=1 2>/dev/null | head -c 12)
if [ "$RESULT" = "Hello world!" ]; then
    echo "OK: Test 1"
else
    echo "FAIL: got '$RESULT'"
    exit 1
fi

echo "===== Test 2: файловая система ====="
sudo mkfs.ext4 "$DEVICE" -q
sudo mkdir -p /mnt/ramtest
sudo mount "$DEVICE" /mnt/ramtest
echo "test" | sudo tee /mnt/ramtest/hello.txt > /dev/null
RESULT=$(sudo cat /mnt/ramtest/hello.txt)
sudo umount /mnt/ramtest
if [ "$RESULT" = "test" ]; then
    echo "OK: Test 2"
else
    echo "FAIL: got '$RESULT'"
    exit 1
fi

echo "===== Test 3: целостность данных ====="
sudo dd if=/dev/urandom of="$DEVICE" bs=4096 count=256 2>/dev/null
sudo dd if="$DEVICE" of=/tmp/snapshot.bin bs=4096 count=256 2>/dev/null
sudo dd if="$DEVICE" bs=4096 count=256 2>/dev/null | diff - /tmp/snapshot.bin \
    && echo "OK: Test 3" || { echo "FAIL: data mismatch"; exit 1; }

echo "===== Все тесты пройдены ====="
