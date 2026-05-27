// SPDX-License-Identifier: GPL-2.0
/*
 * virtblk.c — RAM блочное устройство для ядра Linux 6.18.13-200.fc43.x86_64
 *
 * Модуль регистрирует виртуальное блочное устройство /dev/ram_virtblk,
 * которое хранит данные в оперативной памяти (1 MiB, 2048 секторов по 512 байт).
 * Запись и чтение реализованы через BIO-based API (submit_bio) —
 * без request queue и blk-mq, напрямую через memcpy в/из RAM-буфера.
 */

#include <linux/module.h>      /* Макросы MODULE_LICENSE, module_init, module_exit                        */
#include <linux/kernel.h>      /* pr_info, pr_err и базовые типы ядра                                     */
#include <linux/init.h>        /* Атрибуты __init и __exit                                                */
#include <linux/fs.h>          /* Файловая система — нужна для block_device_operations                    */
#include <linux/blkdev.h>      /* Главный заголовок блочных устройств: gendisk, queue_limits, add_disk... */
#include <linux/blk_types.h>   /* Типы bio, bio_vec, SECTOR_SIZE, op_is_write...                          */
#include <linux/vmalloc.h>     /* vzalloc/vfree — выделение памяти > 1 страницы                           */
#include <linux/string.h>      /* memcpy, snprintf                                                        */

/*
 *  ------------------------------------------------------------------
 *  Макросы, которые записывают строки прямо в секцию .modinfo скомпилированного .ko файла
 *  Можно посмотреть командой modinfo virtblk.ko
 *  ------------------------------------------------------------------
 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Monreale");
MODULE_DESCRIPTION("RAM block device");
MODULE_VERSION("1.0");

/*
 *  ------------------------------------------------------------------
 *  Параметры устройства
 *
 *  SECTOR_SIZE уже определён в <linux/blk_types.h> как (1 << SECTOR_SHIFT), где SECTOR_SHIFT = 9, то есть 2^9 = 512
 *  Используем его напрямую, не переопределяем
 *  ------------------------------------------------------------------
 */
#define DEVICE_NAME	"ram_virtblk"
#define NSECTORS	2048		              /* количество секторов  */
#define DEVICE_SIZE	(NSECTORS * SECTOR_SIZE)  /* = 2048 * 512 = 1 MiB */

/*
 *  ------------------------------------------------------------------
 *  Глобальное состояние
 *
 *  Major говорит, какой драйвер обслуживает устройство
 *  Minor говорит, какое конкретно устройство этого драйвера
 *  ------------------------------------------------------------------
 */
static int major;		           /* Номер типа устройства                         */
static u8 *ram_data;		       /* указатель на наш RAM-буфер                    */
static struct gendisk *ram_disk;   /* главная структура, представляющая диск в ядре */

/*
 *  ------------------------------------------------------------------
 *  Функция ram_virtblk_transfer - копирование данных
 *  При записи копируем из буфера пользователя в наш RAM, при чтении - наоборот
 *
 *  Внутри bio лежит массив bio_vec - каждый bio_vec описывает один кусок памяти пользователя
 *  struct bio_vec {
 *	struct page *bv_page;    - физическая страница памяти
 *	unsigned int bv_len;     - сколько байт в этом куске
 *	unsigned int bv_offset;  - отступ от начала страницы
 *  };
 *  ------------------------------------------------------------------
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

/*
 *  ------------------------------------------------------------------
 *  ram_virtblk_submit_bio - точка входа всех I/O запросов
 *  ------------------------------------------------------------------
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

/*
 *  ------------------------------------------------------------------
 *  ram_virtblk_open, ram_virtblk_release - вызываются когда кто-то открывает/закрывает /dev/ram_virtblk
 *
 *  У нас нет никакой логики - просто логируем в dmesg
 *  ------------------------------------------------------------------
 */
static int ram_virtblk_open(struct gendisk *disk, blk_mode_t mode)
{
	pr_info("%s: device opened\n", DEVICE_NAME);
	return 0;
}

static void ram_virtblk_release(struct gendisk *disk)
{
	pr_info("%s: device released\n", DEVICE_NAME);
}

/*
 *  ------------------------------------------------------------------
 *  Таблица операций
 *
 *  THIS_MODULE - указатель на текущий модуль.
 *  Ядро использует его чтобы не выгружать модуль пока устройство открыто
 *  ------------------------------------------------------------------
 */
static const struct block_device_operations ram_virtblk_ops = {
	.owner		= THIS_MODULE,
	.open		= ram_virtblk_open,
	.release	= ram_virtblk_release,
	.submit_bio	= ram_virtblk_submit_bio,
};

/*
 *  ------------------------------------------------------------------
 *  Инициализация - ram_virtblk_init
 *  ------------------------------------------------------------------
 */
static int __init ram_virtblk_init(void)
{
	int ret;
	/* logical_block_size - минимальный адресуемый блок с точки зрения файловой системы */
	/* physical_block_size - реальный размер блока железа. У нас RAM, поэтому оба 512   */
	struct queue_limits lim = {
		.logical_block_size  = SECTOR_SIZE,
		.physical_block_size = SECTOR_SIZE,
	};

	/* 1. Выделяем RAM под данные                                                                                                */
	/* vzalloc - выделяет память в виртуально непрерывном адресном пространстве ядра                                             */
	/* Используем vzalloc а не kmalloc потому что vzalloc не требует физически непрерывной памяти, что важно для больших буферов */
	ram_data = vzalloc(DEVICE_SIZE);
	if (!ram_data) {
		pr_err("%s: couldn't allocate RAM (%d bytes)\n",
		       DEVICE_NAME, DEVICE_SIZE);
		return -ENOMEM;
	}

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
	ram_disk->major		= major;
	ram_disk->first_minor	= 0;
	ram_disk->minors	= 1;                                    /* только одно устройство, без разделов */
	ram_disk->fops		= &ram_virtblk_ops;
	ram_disk->flags		= GENHD_FL_NO_PART;	                    /* не сканировать таблицу разделов      */
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

/*
 *  ------------------------------------------------------------------
 *  Выгрузка - ram_virtblk_exit
 *  ------------------------------------------------------------------
 */
static void __exit ram_virtblk_exit(void)
{
	del_gendisk(ram_disk);                    /* убираем диск из системы, удаляет /dev/ram_virtblk */
	put_disk(ram_disk);                       /* освобождаем struct gendisk (уменьшаем refcount)   */
	unregister_blkdev(major, DEVICE_NAME);    /* убираем major из /proc/devices                    */
	vfree(ram_data);                          /* освобождаем RAM-буфер                             */
	pr_info("%s: unloaded\n", DEVICE_NAME);
}

/*
 *  ------------------------------------------------------------------
 *  Регистрация точек входа
 *  ------------------------------------------------------------------
 */
module_init(ram_virtblk_init);
module_exit(ram_virtblk_exit);
