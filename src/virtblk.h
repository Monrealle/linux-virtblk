// SPDX-License-Identifier: GPL-2.0 */
/**
 * @file virtblk.h
 * @brief Константы и параметры виртуального блочного устройства.
 *
 * @author Monreale
 * @version 1.0
 * @license GPL-2.0
 */
#ifndef VIRTBLK_H
#define VIRTBLK_H

/** DEVICE_NAME - имя устройства в /dev и в логах ядра */
#define DEVICE_NAME	"ram_virtblk"
/** NSECTORS - количество секторов устройства */
#define NSECTORS 131072
/** DEVICE_SIZE - полный размер устройства в байтах (131072 * 512 = 64 MiB) */
#define DEVICE_SIZE	(NSECTORS * SECTOR_SIZE)

#endif /* VIRTBLK_H */
