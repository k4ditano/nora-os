/* despertar_gpu: la RG35XX trae la GPU (PowerVR SGX544) marcada "disabled"
 * en su device tree, asi que pvrsrvkm nunca la sondea y no se inicializa.
 * Este modulo la marca "okay" EN MEMORIA y crea su dispositivo de plataforma:
 * no toca la tarjeta; al reiniciar, todo vuelve a como estaba. */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/kallsyms.h>

static char *ruta = "/gpu@b0300000";
module_param(ruta, charp, 0444);

static int __init despertar_init(void)
{
	struct device_node *np = of_find_node_by_path(ruta);
	struct platform_device *pdev;
	if (!np) {
		pr_err("despertar_gpu: no existe %s\n", ruta);
		return -ENODEV;
	}
	if (!of_device_is_available(np)) {
		int (*actualizar)(struct device_node *, struct property *) =
			(void *)kallsyms_lookup_name("of_update_property");
		/* de sobra por si la struct del kernel de Actions es mas grande */
		struct property *p = kzalloc(sizeof(*p) + 64, GFP_KERNEL);
		int r;
		if (!actualizar || !p) {
			pr_err("despertar_gpu: sin of_update_property\n");
			return -ENOSYS;
		}
		p->name = kstrdup("status", GFP_KERNEL);
		p->value = kstrdup("okay", GFP_KERNEL);
		p->length = 5;
		r = actualizar(np, p);
		pr_info("despertar_gpu: status -> okay (%d), disponible=%d\n", r, of_device_is_available(np));
	}
	pdev = of_platform_device_create(np, NULL, NULL);
	pr_info("despertar_gpu: dispositivo %s %s\n", ruta, pdev ? "creado" : "NO creado");
	return 0;
}

static void __exit despertar_exit(void) { }

module_init(despertar_init);
module_exit(despertar_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Activa en memoria la GPU desactivada en el device tree de la RG35XX");
