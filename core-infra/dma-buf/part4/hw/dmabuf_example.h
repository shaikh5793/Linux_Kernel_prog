/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#ifndef _DMABUF_EXAMPLE_H
#define _DMABUF_EXAMPLE_H

#define EXPORTER_IOC_MAGIC 'e'
#define EXPORTER_IOC_GET_DMABUF _IO(EXPORTER_IOC_MAGIC, 0)

#define IMPORTER_IOC_MAGIC 'i'
#define IMPORTER_IOC_SET_DMABUF _IOW(IMPORTER_IOC_MAGIC, 0, int)

#endif
