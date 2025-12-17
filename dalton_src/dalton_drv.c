#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_shmem_helper.h>
#include <drm/drm_file.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_managed.h>
#include <drm/drm_device.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_damage_helper.h>
#include <drm/drm_gem_framebuffer_helper.h> // Fixed: Needed for drm_gem_fb_create
#include <drm/drm_format_helper.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

/* 
 * DaltonFix: Zero-Float Color Blindness Correction Driver
 * 
 * Architecture:
 * - Fixed Point 16.16 Arithmetic
 * - DRM Simple Display Pipe
 * - Sysfs dynamic control
 */

#define DRIVER_NAME "dalton_drv"
#define DRIVER_DESC "Daltonism Correction Virtual Display"
#define DRIVER_DATE "20241217"
#define DRIVER_MAJOR 1
#define DRIVER_MINOR 0

// --- Fixed Point Math ---
#define FX_SHIFT 16
#define FX_ONE (1 << FX_SHIFT)
#define FX_FROM_INT(x) ((x) << FX_SHIFT)
#define FX_TO_INT(x) ((x) >> FX_SHIFT)

// fx_mul: (a * b) >> 16. Usons s64 pour eviter overflow temporaire.
static inline s32 fx_mul(s32 a, s32 b) {
    return (s32)(((s64)a * b) >> FX_SHIFT);
}

// Clamp u8
static inline u8 clamp_u8(s32 val) {
    if (val < 0) return 0;
    if (val > 255) return 255;
    return (u8)val;
}

// --- Daltonism Matrices ---

// Matrice 3x3 pour transformation RGB
typedef struct {
    s32 m[3][3];
} fx_matrix_t;

// Standard RGB -> LMS -> Correction -> RGB is complex to precalc in one RGB matrix perfectly without gamma,
// but for kernel shader we use linear approximation matrices.
// Source for matrices: standard CVD simulation matrices (Coblis / Machados algo simplified).
// We store "Simulation" matrices here.
// Correction algo: Dest = Orig + (Orig - Simul) = 2*Orig - Simul. (Daltonization)
// This is linear, so we can precalculate a "Correction Matrix" C = 2*I - S.
// And then interpolate with Identity based on intensity.

// Identity Matrix
static const fx_matrix_t MAT_IDENTITY = {{
    {FX_ONE, 0, 0},
    {0, FX_ONE, 0},
    {0, 0, FX_ONE}
}};

// Protanopia Simulation (Approx)
static const fx_matrix_t MAT_PROTAN_SIM = {{
    {37160, 28384, 0},   // 0.567, 0.433, 0
    {36569, 28966, 0},   // 0.558, 0.442, 0
    {0, 15859, 49676}    // 0, 0.242, 0.758
}};

// Deuteranopia Simulation (Approx)
static const fx_matrix_t MAT_DEUTAN_SIM = {{
    {41156, 24379, 0},   // 0.625, 0.375, 0
    {45875, 19660, 0},   // 0.700, 0.300, 0
    {0, 19660, 45875}    // 0, 0.300, 0.700
}};

// Tritanopia Simulation (Approx)
static const fx_matrix_t MAT_TRITAN_SIM = {{
    {62259, 3276, 0},    // 0.950, 0.050, 0
    {0, 28384, 37160},   // 0, 0.433, 0.567
    {0, 31129, 34406}    // 0, 0.475, 0.525
}};

// Global State
static fx_matrix_t current_correction_matrix;
static int dalton_mode = 0; // 0=Off, 1=Protan, 2=Deutan, 3=Tritan
static int dalton_intensity = 0; // 0 to 100
static DEFINE_SPINLOCK(matrix_lock);

// Helper: Calculate linear interpolation of matrices: Out = A*(1-t) + B*t
static void lerp_matrix(const fx_matrix_t *a, const fx_matrix_t *b, int percent, fx_matrix_t *out) {
    int i, j;
    s32 t = FX_FROM_INT(percent) / 100;
    s32 we = FX_ONE - t; // weight existing (A)
    s32 wn = t;          // weight new (B)

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            out->m[i][j] = fx_mul(a->m[i][j], we) + fx_mul(b->m[i][j], wn);
        }
    }
}

// Recalculate global matrix based on mode & intensity
static void recalc_matrix(void) {
    fx_matrix_t sim_target;
    fx_matrix_t correction_target;
    fx_matrix_t final_mat;
    int i, j;
    
    // Choose simulation matrix
    switch(dalton_mode) {
        case 1: sim_target = MAT_PROTAN_SIM; break;
        case 2: sim_target = MAT_DEUTAN_SIM; break;
        case 3: sim_target = MAT_TRITAN_SIM; break;
        default: 
            spin_lock(&matrix_lock);
            current_correction_matrix = MAT_IDENTITY;
            spin_unlock(&matrix_lock);
            return;
    }

    // Calculate "Correction" target: C = 2*I - S
    // This emphasizes difference.
    for(i=0; i<3; i++) {
        for(j=0; j<3; j++) {
            s32 ident_val = (i == j) ? FX_ONE : 0;
            // C = 2*I - S
            correction_target.m[i][j] = (ident_val * 2) - sim_target.m[i][j];
        }
    }

    // Interpolate between Identity (0% intensity) and Correction Target (100% intensity)
    lerp_matrix(&MAT_IDENTITY, &correction_target, dalton_intensity, &final_mat);

    spin_lock(&matrix_lock);
    current_correction_matrix = final_mat;
    spin_unlock(&matrix_lock);
}

// --- Pixel Shader (Software) ---

static void apply_correction_line(u32 *src, u32 *dst, int width) {
    int x;
    fx_matrix_t mat;
    
    // Copy matrix locally to avoid lock in generic loop
    spin_lock(&matrix_lock);
    mat = current_correction_matrix;
    spin_unlock(&matrix_lock);

    for (x = 0; x < width; x++) {
        u32 p = src[x];
        u8 r = (p >> 16) & 0xFF;
        u8 g = (p >> 8) & 0xFF;
        u8 b = p & 0xFF;
        u8 a = (p >> 24) & 0xFF;   

        // Convert to fixed point
        s32 fr = FX_FROM_INT(r);
        s32 fg = FX_FROM_INT(g);
        s32 fb = FX_FROM_INT(b);

        // Matrix Multiply
        s32 nr = fx_mul(mat.m[0][0], fr) + fx_mul(mat.m[0][1], fg) + fx_mul(mat.m[0][2], fb);
        s32 ng = fx_mul(mat.m[1][0], fr) + fx_mul(mat.m[1][1], fg) + fx_mul(mat.m[1][2], fb);
        s32 nb = fx_mul(mat.m[2][0], fr) + fx_mul(mat.m[2][1], fg) + fx_mul(mat.m[2][2], fb);

        // Convert back and clamp
        dst[x] = (a << 24) | (clamp_u8(FX_TO_INT(nr)) << 16) | (clamp_u8(FX_TO_INT(ng)) << 8) | clamp_u8(FX_TO_INT(nb));
    }
}

// --- DRM & Shadow Buffer ---

struct dalton_device {
    struct drm_device drm;
    struct drm_simple_display_pipe pipe;
    struct drm_connector connector;
    struct drm_mode_config mode_config;
};

static struct dalton_device *dalton_dev;

static const struct drm_mode_config_funcs dalton_mode_config_funcs = {
    .fb_create = drm_gem_fb_create,
    .atomic_check = drm_atomic_helper_check,
    .atomic_commit = drm_atomic_helper_commit,
};

static void dalton_pipe_enable(struct drm_simple_display_pipe *pipe,
                               struct drm_crtc_state *crtc_state,
                               struct drm_plane_state *plane_state) {
    // No HW enable needed
}

static void dalton_pipe_disable(struct drm_simple_display_pipe *pipe) {
    // No HW disable needed
}

static void dalton_pipe_update(struct drm_simple_display_pipe *pipe,
                               struct drm_plane_state *old_state) {
    struct drm_plane_state *state = pipe->plane.state;
    struct drm_framebuffer *fb = state->fb;
    struct drm_gem_shmem_object *shmem;
    u32 *vaddr;
    int h, w;
    struct drm_atomic_helper_damage_iter iter;
    struct drm_rect damage;
    
    if (!fb) return;
    
    // We only process if correction is active
    if (dalton_mode == 0) return;

    shmem = to_drm_gem_shmem_obj(fb->obj[0]);
    vaddr = shmem->vaddr; // Kernel mapping of the buffer
    
    if (!vaddr) return; // Should be vmapped
    
    w = fb->width;
    h = fb->height;

    // Use damage helpers (Standard DRM way to track dirty regions)
    drm_atomic_helper_damage_iter_init(&iter, old_state, state);
    drm_atomic_for_each_plane_damage(&iter, &damage) {
       int y;
       for (y = damage.y1; y < damage.y2; y++) {
           u32 *line_ptr = vaddr + (y * w);
           // In-place correction on the dumb buffer.
           // NOTE: This modifies source. See implementation plan notes.
           apply_correction_line(line_ptr + damage.x1, 
                                 line_ptr + damage.x1, 
                                 damage.x2 - damage.x1);
       }
    }
}

static const struct drm_simple_display_pipe_funcs dalton_pipe_funcs = {
    .enable = dalton_pipe_enable,
    .disable = dalton_pipe_disable,
    .update = dalton_pipe_update,
};

// --- Sysfs ---

static int param_set_mode(const char *val, const struct kernel_param *kp) {
    int ret = param_set_int(val, kp);
    if (ret == 0) recalc_matrix();
    return ret;
}

static int param_set_intensity(const char *val, const struct kernel_param *kp) {
    int ret = param_set_int(val, kp);
    if (ret == 0) recalc_matrix();
    return ret;
}

static const struct kernel_param_ops mode_ops = {
    .set = param_set_mode,
    .get = param_get_int,
};

static const struct kernel_param_ops intensity_ops = {
    .set = param_set_intensity,
    .get = param_get_int,
};

module_param_cb(mode, &mode_ops, &dalton_mode, 0644);
MODULE_PARM_DESC(mode, "Color Blindness Mode: 0=Off, 1=Protan, 2=Deutan, 3=Tritan");

module_param_cb(intensity, &intensity_ops, &dalton_intensity, 0644);
MODULE_PARM_DESC(intensity, "Correction Intensity: 0-100%");

// --- Init/Exit ---

#include <linux/platform_device.h>

// ... (Start of init section)

static struct platform_device *dalton_pdev;

static const struct file_operations dalton_fops = {
    .owner = THIS_MODULE,
    .open = drm_open,
    .release = drm_release,
    .unlocked_ioctl = drm_ioctl,
    .compat_ioctl = drm_compat_ioctl,
    .poll = drm_poll,
    .read = drm_read,
    .llseek = no_llseek,
    .mmap = drm_gem_mmap,
};

static const struct drm_driver dalton_driver = {
    .driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,
    .name = DRIVER_NAME,
    .desc = DRIVER_DESC,
    .date = DRIVER_DATE,
    .major = DRIVER_MAJOR,
    .minor = DRIVER_MINOR,
    .fops = &dalton_fops,
    .dumb_create = drm_gem_shmem_dumb_create,
};

static int __init dalton_init(void) {
    int ret;
    struct drm_device *drm;
    
    recalc_matrix();

    // Register a virtual platform device
    dalton_pdev = platform_device_register_simple("dalton_vdev", -1, NULL, 0);
    if (IS_ERR(dalton_pdev)) return PTR_ERR(dalton_pdev);

    // Allocate DRM device managed by the platform device
    dalton_dev = devm_drm_dev_alloc(&dalton_pdev->dev, &dalton_driver, struct dalton_device, drm);
    if (IS_ERR(dalton_dev)) {
        ret = PTR_ERR(dalton_dev);
        goto unreg_pdev;
    }

    drm = &dalton_dev->drm;
    
    // Config Modeset
    drm_mode_config_init(drm);
    drm->mode_config.funcs = &dalton_mode_config_funcs;
    drm->mode_config.min_width = 320;
    drm->mode_config.min_height = 240;
    drm->mode_config.max_width = 3840;
    drm->mode_config.max_height = 2160;

    static const uint32_t formats[] = { DRM_FORMAT_XRGB8888, DRM_FORMAT_ARGB8888 };
    ret = drm_simple_display_pipe_init(drm, &dalton_dev->pipe, &dalton_pipe_funcs,
                                     formats, ARRAY_SIZE(formats), NULL, &dalton_dev->connector);
    if (ret) goto cleanup_mode; // Note: devm handles drm_dev_put

    ret = drm_dev_register(drm, 0);
    if (ret) goto cleanup_mode;
    
    DRM_INFO("DaltonFix Driver Initialized.\n");
    return 0;

cleanup_mode:
    // drm_mode_config_cleanup(drm); // managed? No, usually not managed.
    // Actually drm_mode_config_init is NOT managed by default unless drmm_mode_config_init used.
    // But devm_drm_dev_alloc cleans up the struct.
    // We should manual cleanup mode_config if register fails.
    drm_dev_unregister(drm); // Only if registered? No, we are in error path.
    // If pipe init failed, we haven't registered.
    // Just cleanup mode config.
    drm_mode_config_cleanup(drm);
unreg_pdev:
    platform_device_unregister(dalton_pdev);
    return ret;
}

static void __exit dalton_exit(void) {
    if (dalton_dev) {
        drm_dev_unregister(&dalton_dev->drm);
        drm_mode_config_cleanup(&dalton_dev->drm);
        // devm release will free dalton_dev when pdev is unregistered
    }
    if (dalton_pdev) {
        platform_device_unregister(dalton_pdev);
    }
    DRM_INFO("DaltonFix Driver Unloaded.\n");
}


module_init(dalton_init);
module_exit(dalton_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Antigravity");
