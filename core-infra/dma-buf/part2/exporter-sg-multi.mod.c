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

KSYMTAB_DATA(dmabuf_sg_multi_exported, "_gpl", "");

SYMBOL_CRC(dmabuf_sg_multi_exported, 0x80256aa3, "_gpl");

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xd648ae19, "sg_free_table" },
	{ 0xbd03ed67, "random_kmalloc_seed" },
	{ 0x01218420, "kmalloc_caches" },
	{ 0xa76e789c, "__kmalloc_cache_noprof" },
	{ 0xb09bc67d, "sg_alloc_table" },
	{ 0x0d86b9c8, "sg_next" },
	{ 0xfd64d62a, "dma_map_sgtable" },
	{ 0xaeb80711, "dma_buf_put" },
	{ 0x86e82a80, "alloc_pages_noprof" },
	{ 0xbd03ed67, "vmemmap_base" },
	{ 0x1bdf2bc8, "sme_me_mask" },
	{ 0x5fc55113, "__default_kernel_pte_mask" },
	{ 0xdd42422e, "vmap" },
	{ 0x40a621c5, "snprintf" },
	{ 0x74ff8d55, "dma_buf_export" },
	{ 0xd272d446, "__stack_chk_fail" },
	{ 0xd272d446, "__fentry__" },
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0xe8213e80, "_printk" },
	{ 0xf1de9e85, "vunmap" },
	{ 0xd128f83f, "__free_pages" },
	{ 0xcb8b6ec6, "kfree" },
	{ 0x90a48d82, "__ubsan_handle_out_of_bounds" },
	{ 0xc377b758, "dma_unmap_sg_attrs" },
	{ 0xc773217c, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0xd648ae19,
	0xbd03ed67,
	0x01218420,
	0xa76e789c,
	0xb09bc67d,
	0x0d86b9c8,
	0xfd64d62a,
	0xaeb80711,
	0x86e82a80,
	0xbd03ed67,
	0x1bdf2bc8,
	0x5fc55113,
	0xdd42422e,
	0x40a621c5,
	0x74ff8d55,
	0xd272d446,
	0xd272d446,
	0xd272d446,
	0xe8213e80,
	0xf1de9e85,
	0xd128f83f,
	0xcb8b6ec6,
	0x90a48d82,
	0xc377b758,
	0xc773217c,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"sg_free_table\0"
	"random_kmalloc_seed\0"
	"kmalloc_caches\0"
	"__kmalloc_cache_noprof\0"
	"sg_alloc_table\0"
	"sg_next\0"
	"dma_map_sgtable\0"
	"dma_buf_put\0"
	"alloc_pages_noprof\0"
	"vmemmap_base\0"
	"sme_me_mask\0"
	"__default_kernel_pte_mask\0"
	"vmap\0"
	"snprintf\0"
	"dma_buf_export\0"
	"__stack_chk_fail\0"
	"__fentry__\0"
	"__x86_return_thunk\0"
	"_printk\0"
	"vunmap\0"
	"__free_pages\0"
	"kfree\0"
	"__ubsan_handle_out_of_bounds\0"
	"dma_unmap_sg_attrs\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "493E15362F53273A6BADF7E");
