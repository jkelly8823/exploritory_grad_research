		pr_err("Overwrite did not happen, but no BUG?!\n");
	else {
		pr_err("list_add() corruption not detected!\n");
		pr_expected_config(CONFIG_DEBUG_LIST);
	}
}

static void lkdtm_CORRUPT_LIST_DEL(void)
		pr_err("Overwrite did not happen, but no BUG?!\n");
	else {
		pr_err("list_del() corruption not detected!\n");
		pr_expected_config(CONFIG_DEBUG_LIST);
	}
}

/* Test that VMAP_STACK is actually allocating with a leading guard page */