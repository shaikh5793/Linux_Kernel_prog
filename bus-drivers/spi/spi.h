/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#define W25_ID		0x9F	/* read manufacturer id */
#define W25_WREN	0x06	/* latch the write enable */
#define W25_WRDI	0X04	/* reset the write enable */
#define W25_RDSR	0X05
#define W25_WRSR	0X01
#define W25_READ	0X03
#define W25_WRITE	0X02
#define W25_MN_ID	0XEF

#define W25_SR_nRDY	0X01
#define W25_SR_WEN	0X02

#define W25_SEC_ERASE	0X20

#define W25_MAXADDRLEN	3


#define W25_TIMEOUT	25

#define IO_LIMIT	256

struct W25_priv {
	char			name[15];
	struct spi_device	*spi;
	struct kobject		*kobj;
	long int		offset;
	unsigned		size;
	unsigned		page_size;
	unsigned		addr_width;
	unsigned		block;
	unsigned 		sector;
	unsigned		page;
};
