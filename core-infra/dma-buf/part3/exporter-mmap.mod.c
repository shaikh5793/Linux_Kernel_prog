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
	{ 0xd648ae19, "sg_free_table" },
	{ 0x39817072, "misc_deregister" },
	{ 0xaeb80711, "dma_buf_put" },
	{ 0xbd03ed67, "random_kmalloc_seed" },
	{ 0x01218420, "kmalloc_caches" },
	{ 0xa76e789c, "__kmalloc_cache_noprof" },
	{ 0xb09bc67d, "sg_alloc_table" },
	{ 0xc45d298e, "is_vmalloc_addr" },
	{ 0xbd03ed67, "vmemmap_base" },
	{ 0xe65ef89e, "dma_map_page_attrs" },
	{ 0x298debd6, "dev_driver_string" },
	{ 0x75738bed, "__warn_printk" },
	{ 0xe4de56b4, "__ubsan_handle_load_invalid_value" },
	{ 0x74ff8d55, "dma_buf_export" },
	{ 0x0b0257b7, "misc_register" },
	{ 0xd272d446, "__fentry__" },
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0xe8213e80, "_printk" },
	{ 0x83121b05, "dma_buf_fd" },
	{ 0x092a35a2, "_copy_to_user" },
	{ 0xd272d446, "__stack_chk_fail" },
	{ 0xcb8b6ec6, "kfree" },
	{ 0xbd03ed67, "phys_base" },
	{ 0xbd03ed67, "page_offset_base" },
	{ 0xeee35516, "remap_pfn_range" },
	{ 0xe7f6a628, "dma_unmap_page_attrs" },
	{ 0xc773217c, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0xd648ae19,
	0x39817072,
	0xaeb80711,
	0xbd03ed67,
	0x01218420,
	0xa76e789c,
	0xb09bc67d,
	0xc45d298e,
	0xbd03ed67,
	0xe65ef89e,
	0x298debd6,
	0x75738bed,
	0xe4de56b4,
	0x74ff8d55,
	0x0b0257b7,
	0xd272d446,
	0xd272d446,
	0xe8213e80,
	0x83121b05,
	0x092a35a2,
	0xd272d446,
	0xcb8b6ec6,
	0xbd03ed67,
	0xbd03ed67,
	0xeee35516,
	0xe7f6a628,
	0xc773217c,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"sg_free_table\0"
	"misc_deregister\0"
	"dma_buf_put\0"
	"random_kmalloc_seed\0"
	"kmalloc_caches\0"
	"__kmalloc_cache_noprof\0"
	"sg_alloc_table\0"
	"is_vmalloc_addr\0"
	"vmemmap_base\0"
	"dma_map_page_attrs\0"
	"dev_driver_string\0"
	"__warn_printk\0"
	"__ubsan_handle_load_invalid_value\0"
	"dma_buf_export\0"
	"misc_register\0"
	"__fentry__\0"
	"__x86_return_thunk\0"
	"_printk\0"
	"dma_buf_fd\0"
	"_copy_to_user\0"
	"__stack_chk_fail\0"
	"kfree\0"
	"phys_base\0"
	"page_offset_base\0"
	"remap_pfn_range\0"
	"dma_unmap_page_attrs\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "2F8287C67F4F2E23CE509B6");
