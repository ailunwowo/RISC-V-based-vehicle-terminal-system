// SPDX-License-Identifier: GPL-2.0-only
/*
 * accelerometer sensor qma6100p driver source file
 *
 * Copyright Leapfive Ltd.
 *
 * Author: Peter.Li <peter.li@leapfive.com>
 */
#include "qma6100p.h"

static int qma6100p_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	printk("probe qma6100p success!\n");
	return 0;
}

static int qma6100p_remove(struct i2c_client *client)
{
	printk("%s success\n", __func__);
	return 0;
}

static const struct of_device_id qma6100p_dt_ids[] = {
	{ .compatible = "leapfive,qma6100p" },
	{},
};

static struct i2c_driver qma6100p_i2c_driver = {
	.driver = {
		.name =	"qma6100p",
		.of_match_table = qma6100p_dt_ids,
	},
	.probe  = qma6100p_probe,
	.remove = qma6100p_remove,
};

static int __init qma6100p_init(void)
{
	int status;

	status = i2c_add_driver(&qma6100p_i2c_driver);
	if (status < 0)
		pr_err("register qma6100p drv failed!\n");
	printk("register qma6100p drv success!\n");

	return status;
}

static void __exit qma6100p_exit(void)
{
	printk("qma6100p driver unregister success!\n");
	i2c_del_driver(&qma6100p_i2c_driver);
}

module_init(qma6100p_init);
module_exit(qma6100p_exit);

MODULE_LICENSE("GPL");
