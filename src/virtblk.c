// SPDX-License-Identifier: GPL-2.0
/**
 * @file virtblk.c
 * @brief RAM блочное устройство для ядра Linux.
 *
 * Модуль регистрирует виртуальное блочное устройство /dev/ram_virtblk,
 * которое хранит данные в оперативной памяти (64 MiB, 131072 секторов по 512 байт).
 * Запись и чтение реализованы через BIO-based API (submit_bio) —
 * без request queue и blk-mq, напрямую через memcpy в/из RAM-буфера.
 * 
 * @author Monreale
 * @version 1.0
 * @license GPL-2.0
 */

#include <linux/module.h>      /* Макросы MODULE_LICENSE, module_init, module_exit                        */
#include <linux/kernel.h>      /* pr_info, pr_err и базовые типы ядра                                     */
#include <linux/init.h>        /* Атрибуты __init и __exit                                                */
#include <linux/fs.h>          /* Файловая система — нужна для block_device_operations                    */
#include <linux/blkdev.h>      /* Главный заголовок блочных устройств: gendisk, queue_limits, add_disk... */
#include <linux/blk_types.h>   /* Типы bio, bio_vec, SECTOR_SIZE, op_is_write...                          */
#include <linux/vmalloc.h>     /* vzalloc/vfree — выделение памяти > 1 страницы                           */
#include <linux/string.h>      /* memcpy, snprintf                                                        */
#include "virtblk.h"

/*
 *  Макросы, которые записывают строки прямо в секцию .modinfo скомпилированного .ko файла
 *  Можно посмотреть командой modinfo virtblk.ko
 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Monreale");
MODULE_DESCRIPTION("RAM block device");
MODULE_VERSION("1.0");

static int major;		           /**< major-номер драйвера, выдаётся ядром при регистрации */
static u8 *ram_data;		       /**< RAM-буфер, хранящий данные устройства                */
static struct gendisk *ram_disk;   /**< главная структура, представляющая диск в ядре        */

/**
 * ram_virtblk_transfer() - скопировать данные между bio_vec и RAM-буфером
 * @param bvec: дескриптор одного сегмента BIO (страница, смещение, длина)
 * @param offset: байтовое смещение в RAM-буфере устройства
 * @param write: true — запись в RAM, false — чтение из RAM
 *
 * Маппит физическую страницу через kmap_local_page(), выполняет memcpy()
 * и сразу размаппирует. Вызывается для каждого сегмента из
 * ram_virtblk_submit_bio().
 */
static void ram_virtblk_transfer(struct bio_vec bvec, loff_t offset, bool write)
{
	/* kmap_local_page(bvec.bv_page) маппит физическую страницу и возвращает виртуальный адрес её начала */
	/* Прибавляем bv_offset чтобы попасть точно на наши данные                                           */
	void *buf = kmap_local_page(bvec.bv_page) + bvec.bv_offset;

	if (write)
		memcpy(ram_data + offset, buf, bvec.bv_len);
	else
		memcpy(buf, ram_data + offset, bvec.bv_len);

	/* kunmap_local() - размаппим страницу обратно, иначе утечка маппингов */
	/* Передаём адрес начала страницы. Вычитаем bv_offset обратно          */
	kunmap_local(buf - bvec.bv_offset);
}

/**
 * ram_virtblk_submit_bio() - точка входа всех I/O запросов к устройству
 * @param bio: указатель на BIO-запрос от блочного слоя ядра
 *
 * Вычисляет байтовое смещение из номера сектора, проверяет что запрос
 * не выходит за пределы устройства, затем обходит сегменты BIO через
 * bio_for_each_segment() и для каждого вызывает ram_virtblk_transfer().
 * Завершает запрос вызовом bio_endio().
 *
 * Контекст: процесс-контекст, может спать.
 */
static void ram_virtblk_submit_bio(struct bio *bio)
{
	struct bio_vec bvec;
	struct bvec_iter iter;

	/* bio->bi_iter.bi_sector - номер начального сектора запроса        */
	/* Умножаем на 512 чтобы получить байтовый offset в нашем буфере    */
	/* bio_op(bio) возвращает операцию: REQ_OP_READ, REQ_OP_WRITE и т.д */
	/* op_is_write() проверяет является ли операция записью             */
	loff_t offset = bio->bi_iter.bi_sector * SECTOR_SIZE;
	bool write = op_is_write(bio_op(bio));

	/* Защита от запросов за пределы устройства                                                 */
	/* Если кто-то попросит прочитать сектор 9999 при размере 2048 секторов - возвращаем ошибку */
	if (offset + bio->bi_iter.bi_size > DEVICE_SIZE) {
		pr_err("%s: I/O out of range (offset=%lld, size=%u)\n",
		       DEVICE_NAME, offset, bio->bi_iter.bi_size);
		bio_io_error(bio);
		return;
	}

	/* bio_for_each_segment - макрос-итератор                                                  */
	/* Обходим каждый bio_vec и копируем данные, сдвигая offset на размер скопированного куска */
	bio_for_each_segment(bvec, bio, iter) {
		ram_virtblk_transfer(bvec, offset, write);
		offset += bvec.bv_len;
	}

	bio_endio(bio); /* говорим ядру "запрос выполнен" */
}

/**
 * ram_virtblk_open() - обработчик открытия блочного устройства
 * @param gd: указатель на gendisk устройства
 * @param mode: режим открытия (BLK_OPEN_READ, BLK_OPEN_WRITE и др.)
 *
 * Логирует событие в dmesg. Дополнительная инициализация не требуется,
 * так как устройство полностью готово к работе после загрузки модуля.
 *
 * Return: всегда 0 (успех).
 */
static int ram_virtblk_open(struct gendisk *gd, blk_mode_t mode)
{
	pr_info("%s: device opened\n", DEVICE_NAME);
	return 0;
}

/**
 * ram_virtblk_release() - обработчик закрытия блочного устройства
 * @param gd: указатель на gendisk устройства
 *
 * Логирует событие в dmesg. Освобождение ресурсов не требуется —
 * RAM-буфер живёт до выгрузки модуля.
 */
static void ram_virtblk_release(struct gendisk *gd)
{
	pr_info("%s: device released\n", DEVICE_NAME);
}

/** @brief Таблица операций блочного устройства. */
static const struct block_device_operations ram_virtblk_ops = {
	.owner = THIS_MODULE,
	.open = ram_virtblk_open,
	.release = ram_virtblk_release,
	.submit_bio	= ram_virtblk_submit_bio,
};

/**
 * ram_virtblk_init() - инициализация модуля
 *
 * Выполняет последовательно:
 *   1. vzalloc() — выделение RAM-буфера под данные устройства;
 *   2. register_blkdev() — регистрация драйвера, получение major-номера;
 *   3. blk_alloc_disk() — создание struct gendisk с заданными queue_limits;
 *   4. настройка полей gendisk и регистрация диска через add_disk().
 *
 * При ошибке на любом шаге откатывает все предыдущие шаги через goto.
 *
 * Return: 0 при успехе, отрицательный код ошибки при неудаче.
 */
static int __init ram_virtblk_init(void)
{
	int ret;
	/* logical_block_size - минимальный адресуемый блок с точки зрения файловой системы */
	/* physical_block_size - реальный размер блока железа. У нас RAM, поэтому оба 512   */
	struct queue_limits lim = {
		.logical_block_size = SECTOR_SIZE,
		.physical_block_size = SECTOR_SIZE,
	};

	/* 1. Выделяем RAM под данные                                                                                                */
	/* vzalloc - выделяет память в виртуально непрерывном адресном пространстве ядра                                             */
	/* Используем vzalloc а не kmalloc потому что vzalloc не требует физически непрерывной памяти, что важно для больших буферов */
	ram_data = vzalloc(DEVICE_SIZE);
	if (!ram_data)
		return -ENOMEM;

	/* 2. Регистрируем драйвер в ядре и получаем major номер */
	/* Результат виден в /proc/devices                       */
	major = register_blkdev(0, DEVICE_NAME);
	if (major < 0) {
		pr_err("%s: failed to register block device\n", DEVICE_NAME);
		ret = major;
		goto err_free_ram;
	}

	/* 3. Создаём struct gendisk и очередь запросов с нашими лимитами */
	/* NUMA_NO_NODE — не привязываемся к конкретному NUMA-узлу        */
	ram_disk = blk_alloc_disk(&lim, NUMA_NO_NODE);
	if (IS_ERR(ram_disk)) {
		ret = PTR_ERR(ram_disk);
		pr_err("%s: failed to allocate disk\n", DEVICE_NAME);
		goto err_unreg_virtblk;
	}

	/* 4. Настраиваем gendisk         */
	/* set_capacity принимает секторы */
	ram_disk->major	= major;
	ram_disk->first_minor = 0;
	ram_disk->minors = 1;                                       /* только одно устройство, без разделов */
	ram_disk->fops = &ram_virtblk_ops;
	ram_disk->flags	= GENHD_FL_NO_PART;	                        /* не сканировать таблицу разделов      */
	snprintf(ram_disk->disk_name, DISK_NAME_LEN, DEVICE_NAME);
	set_capacity(ram_disk, NSECTORS);

	/* 5. Регистрируем диск в подсистеме блочных устройств                                     */
	/* Появляется /dev/ram_virtblk и устройство готово к использованию                         */
	/* Если что-то пошло не так на шаге N, нужно откатить всё что было сделано на шагах 1..N-1 */
	/* goto раскручивает инициализацию в обратном порядке                                      */
	ret = add_disk(ram_disk);
	if (ret) {
		pr_err("%s: failed to add disk\n", DEVICE_NAME);
		goto err_put_disk;
	}

	pr_info("%s: loaded, major=%d, size=%d KiB\n",
		DEVICE_NAME, major, DEVICE_SIZE / 1024);
	return 0;

err_put_disk:
	put_disk(ram_disk);
err_unreg_virtblk:
	unregister_blkdev(major, DEVICE_NAME);
err_free_ram:
	vfree(ram_data);
	return ret;
}

/**
 * ram_virtblk_exit() - выгрузка модуля
 *
 * Откатывает все шаги инициализации в обратном порядке:
 * del_gendisk() -> put_disk() -> unregister_blkdev() -> vfree().
 */
static void __exit ram_virtblk_exit(void)
{
	del_gendisk(ram_disk);                    /* убираем диск из системы, удаляет /dev/ram_virtblk */
	put_disk(ram_disk);                       /* освобождаем struct gendisk (уменьшаем refcount)   */
	unregister_blkdev(major, DEVICE_NAME);    /* убираем major из /proc/devices                    */
	vfree(ram_data);                          /* освобождаем RAM-буфер                             */
	pr_info("%s: unloaded\n", DEVICE_NAME);
}

/* Регистрация точек входа */
module_init(ram_virtblk_init);
module_exit(ram_virtblk_exit);
