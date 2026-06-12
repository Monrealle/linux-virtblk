/* SPDX-License-Identifier: GPL-2.0 */
/**
 * @file virtblk.h
 * @brief Константы и параметры виртуального блочного устройства.
 *
 * @author Monreale
 * @version 1.0
 */
#ifndef VIRTBLK_H
#define VIRTBLK_H

/** RAM_VIRTBLK_NAME - имя устройства в /dev и в логах ядра */
#define RAM_VIRTBLK_NAME "ram_virtblk"
/** RAM_VIRTBLK_SEC_SIZE - размер одного сектора в байтах */
#define RAM_VIRTBLK_SEC_SIZE 512
/** RAM_VIRTBLK_NSECTORS - количество секторов устройства */
#define RAM_VIRTBLK_NSECTORS 131072
/** RAM_VIRTBLK_SIZE - полный размер устройства в байтах (131072 * 512 = 64 MiB) */
#define RAM_VIRTBLK_SIZE (RAM_VIRTBLK_NSECTORS * RAM_VIRTBLK_SEC_SIZE)

#endif /* VIRTBLK_H */
