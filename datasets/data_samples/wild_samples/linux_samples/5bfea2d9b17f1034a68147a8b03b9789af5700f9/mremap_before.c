	for (; old_addr < old_end; old_addr += extent, new_addr += extent) {
		cond_resched();
		next = (old_addr + PMD_SIZE) & PMD_MASK;
		/* even if next overflowed, extent below will be ok */
		extent = next - old_addr;
		if (extent > old_end - old_addr)
			extent = old_end - old_addr;
		old_pmd = get_old_pmd(vma->vm_mm, old_addr);
		if (!old_pmd)
			continue;
		new_pmd = alloc_new_pmd(vma->vm_mm, vma, new_addr);
		if (!new_pmd)
			break;
		if (is_swap_pmd(*old_pmd) || pmd_trans_huge(*old_pmd)) {
			if (extent == HPAGE_PMD_SIZE) {
				bool moved;
				/* See comment in move_ptes() */
				if (need_rmap_locks)
					take_rmap_locks(vma);
				moved = move_huge_pmd(vma, old_addr, new_addr,
						    old_end, old_pmd, new_pmd);
				if (need_rmap_locks)
					drop_rmap_locks(vma);
				if (moved)
					continue;
			}
			split_huge_pmd(vma, old_pmd, old_addr);
			if (pmd_trans_unstable(old_pmd))
				continue;
		} else if (extent == PMD_SIZE) {
#ifdef CONFIG_HAVE_MOVE_PMD
			/*
			 * If the extent is PMD-sized, try to speed the move by
			 * moving at the PMD level if possible.
			 */
			bool moved;

			if (need_rmap_locks)
				take_rmap_locks(vma);
			moved = move_normal_pmd(vma, old_addr, new_addr,
					old_end, old_pmd, new_pmd);
			if (need_rmap_locks)
				drop_rmap_locks(vma);
			if (moved)
				continue;
#endif
		}

		if (pte_alloc(new_vma->vm_mm, new_pmd))
			break;
		next = (new_addr + PMD_SIZE) & PMD_MASK;
		if (extent > next - new_addr)
			extent = next - new_addr;
		move_ptes(vma, old_pmd, old_addr, old_addr + extent, new_vma,
			  new_pmd, new_addr, need_rmap_locks);
	}