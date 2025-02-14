#include <linux/module.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
	if (err)
		return err;

	err = dw_dma_probe(chip, pdata);
	if (err)
		goto err_dw_dma_probe;

	return 0;

err_dw_dma_probe:
	clk_disable_unprepare(chip->clk);
	return err;
}

		of_dma_controller_free(pdev->dev.of_node);

	dw_dma_remove(chip);
	clk_disable_unprepare(chip->clk);

	return 0;
}