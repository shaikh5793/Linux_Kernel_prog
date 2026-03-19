#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0xd272d446, "__fentry__" },
	{ 0x80256aa3, "dmabuf_sg_multi_exported" },
	{ 0x3f6215a0, "platform_device_register_full" },
	{ 0xf3bd9e1a, "dma_set_mask" },
	{ 0xf3bd9e1a, "dma_set_coherent_mask" },
	{ 0x1f3685fd, "dma_buf_attach" },
	{ 0x2850bc93, "dma_buf_map_attachment" },
	{ 0x01b49237, "dma_buf_detach" },
	{ 0x0d86b9c8, "sg_next" },
	{ 0x05035eb2, "dma_buf_unmap_attachment" },
	{ 0xe36d0a35, "dma_buf_vmap" },
	{ 0xe4de56b4, "__ubsan_handle_load_invalid_value" },
	{ 0x53a01c7a, "dma_buf_vunmap" },
	{ 0xd272d446, "__stack_chk_fail" },
	{ 0xdbbf0721, "platform_device_unregister" },
	{ 0xe8213e80, "_printk" },
	{ 0xc773217c, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0xd272d446,
	0xd272d446,
	0x80256aa3,
	0x3f6215a0,
	0xf3bd9e1a,
	0xf3bd9e1a,
	0x1f3685fd,
	0x2850bc93,
	0x01b49237,
	0x0d86b9c8,
	0x05035eb2,
	0xe36d0a35,
	0xe4de56b4,
	0x53a01c7a,
	0xd272d446,
	0xdbbf0721,
	0xe8213e80,
	0xc773217c,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"__x86_return_thunk\0"
	"__fentry__\0"
	"dmabuf_sg_multi_exported\0"
	"platform_device_register_full\0"
	"dma_set_mask\0"
	"dma_set_coherent_mask\0"
	"dma_buf_attach\0"
	"dma_buf_map_attachment\0"
	"dma_buf_detach\0"
	"sg_next\0"
	"dma_buf_unmap_attachment\0"
	"dma_buf_vmap\0"
	"__ubsan_handle_load_invalid_value\0"
	"dma_buf_vunmap\0"
	"__stack_chk_fail\0"
	"platform_device_unregister\0"
	"_printk\0"
	"module_layout\0"
;

MODULE_INFO(depends, "exporter-sg-multi");


MODULE_INFO(srcversion, "736E9AE8D1601A4769FE998");
