/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

/*
 * 1. magic no(8 bit)
 * 2. seq no(8 bit)
 * 3. Parameter data size 
 * 4. Direction of transfer
 */


#define EEPROM_IOCTL_MAGIC 'e'
#define EEPROM_IOCTL_SET_PAGE_OFFSET _IOW(EEPROM_IOCTL_MAGIC, 1, unsigned int)
#define EEPROM_IOCTL_PAGE_READ       _IOR(EEPROM_IOCTL_MAGIC, 2, char *)
#define EEPROM_IOCTL_PAGE_WRITE      _IOW(EEPROM_IOCTL_MAGIC, 3, char *)

