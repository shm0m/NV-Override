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
	{ 0x08c834d9, "drm_atomic_helper_damage_iter_next" },
	{ 0xde338d9a, "_raw_spin_lock" },
	{ 0xde338d9a, "_raw_spin_unlock" },
	{ 0xd272d446, "__stack_chk_fail" },
	{ 0x90a48d82, "__ubsan_handle_out_of_bounds" },
	{ 0xe73fefc0, "param_set_int" },
	{ 0x29010c7e, "platform_device_register_full" },
	{ 0x8de7b5f9, "__devm_drm_dev_alloc" },
	{ 0xe465eecf, "drmm_mode_config_init" },
	{ 0x0e0c9f43, "drm_simple_display_pipe_init" },
	{ 0xc44dc7c6, "drm_dev_register" },
	{ 0x22589908, "drm_gem_shmem_dumb_create" },
	{ 0x81d6dd8f, "noop_llseek" },
	{ 0x07d48f62, "drm_read" },
	{ 0xc6a4049b, "drm_poll" },
	{ 0x6dd6502a, "drm_ioctl" },
	{ 0x6dd6502a, "drm_compat_ioctl" },
	{ 0xc1465492, "drm_gem_mmap" },
	{ 0xee57c96f, "drm_open" },
	{ 0xee57c96f, "drm_release" },
	{ 0x52b17862, "param_get_int" },
	{ 0x208a2ab7, "drm_gem_fb_create" },
	{ 0x498f2fae, "drm_atomic_helper_check" },
	{ 0x24b08e18, "drm_atomic_helper_commit" },
	{ 0xd272d446, "__fentry__" },
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0xeb08a89e, "drm_dev_unregister" },
	{ 0x6052a5b2, "drm_mode_config_cleanup" },
	{ 0x71e3d3cc, "platform_device_unregister" },
	{ 0xe8213e80, "_printk" },
	{ 0x1d3414d5, "drm_atomic_helper_damage_iter_init" },
	{ 0xba157484, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0x08c834d9,
	0xde338d9a,
	0xde338d9a,
	0xd272d446,
	0x90a48d82,
	0xe73fefc0,
	0x29010c7e,
	0x8de7b5f9,
	0xe465eecf,
	0x0e0c9f43,
	0xc44dc7c6,
	0x22589908,
	0x81d6dd8f,
	0x07d48f62,
	0xc6a4049b,
	0x6dd6502a,
	0x6dd6502a,
	0xc1465492,
	0xee57c96f,
	0xee57c96f,
	0x52b17862,
	0x208a2ab7,
	0x498f2fae,
	0x24b08e18,
	0xd272d446,
	0xd272d446,
	0xeb08a89e,
	0x6052a5b2,
	0x71e3d3cc,
	0xe8213e80,
	0x1d3414d5,
	0xba157484,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"drm_atomic_helper_damage_iter_next\0"
	"_raw_spin_lock\0"
	"_raw_spin_unlock\0"
	"__stack_chk_fail\0"
	"__ubsan_handle_out_of_bounds\0"
	"param_set_int\0"
	"platform_device_register_full\0"
	"__devm_drm_dev_alloc\0"
	"drmm_mode_config_init\0"
	"drm_simple_display_pipe_init\0"
	"drm_dev_register\0"
	"drm_gem_shmem_dumb_create\0"
	"noop_llseek\0"
	"drm_read\0"
	"drm_poll\0"
	"drm_ioctl\0"
	"drm_compat_ioctl\0"
	"drm_gem_mmap\0"
	"drm_open\0"
	"drm_release\0"
	"param_get_int\0"
	"drm_gem_fb_create\0"
	"drm_atomic_helper_check\0"
	"drm_atomic_helper_commit\0"
	"__fentry__\0"
	"__x86_return_thunk\0"
	"drm_dev_unregister\0"
	"drm_mode_config_cleanup\0"
	"platform_device_unregister\0"
	"_printk\0"
	"drm_atomic_helper_damage_iter_init\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "26636749CC645FF5F134D97");
