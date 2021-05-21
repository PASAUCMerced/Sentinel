#include <asm/pgalloc.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <linux/badger_trap.h>
#include <linux/syscalls.h>
#include <linux/hugetlb.h>
#include <linux/kernel.h>


/*
 * This syscall is generic way of setting up badger trap.
 * There are three options to start badger trap.
 * (1) 	option > 0: provide all process names with number of processes.
 * 	This will mark the process names for badger trap to start when any
 * 	process with names specified will start.
 *
 * (2) 	option == 0: starts badger trap for the process calling the syscall itself.
 *  	This requires binary to be updated for the workload to call badger trap. This
 *  	option is useful when you want to skip the warmup phase of the program. You can
 *  	introduce the syscall in the program to invoke badger trap after that phase.
 *
 * (3) 	option < 0: provide all pid with number of processes. This will start badger
 *  	trap for all pids provided immidiately.
 *
 *  Note: 	(1) will allow all the child processes to be marked for badger trap when
 *  		forked from a badger trap process.

 *		(2) and (3) will not mark the already spawned child processes for badger
 *		trap when you mark the parent process for badger trap on the fly. But (2) and (3)
 *		will mark all child spwaned from the parent process after being marked for badger trap. 
 */
SYSCALL_DEFINE3(init_badger_trap, const char __user**, process_name, unsigned long, num_procs, int, option)
{
	unsigned int i;
	char *temp;
	unsigned long ret=0;
	char proc[MAX_NAME_LEN];
	struct task_struct * tsk;
	unsigned long pid;

	if(option > 0)
	{
		for(i=0; i<CONFIG_NR_CPUS; i++)
		{
			if(i<num_procs)
				ret = strncpy_from_user(proc, process_name[i], MAX_NAME_LEN);
			else
				temp = strncpy(proc,"",MAX_NAME_LEN);
			temp = strncpy(badger_trap_process[i], proc, MAX_NAME_LEN-1);
		}
	}

	// All other inputs ignored
	if(option == 0)
	{
		current->mm->badger_trap_en = 1;
		badger_trap_init(current->mm);
	}

	if(option < 0)
	{
		for(i=0; i<CONFIG_NR_CPUS; i++)
		{
			if(i<num_procs)
			{
				ret = kstrtoul(process_name[i],10,&pid);
				if(ret == 0)
				{
					tsk = find_task_by_vpid(pid);
					tsk->mm->badger_trap_en = 1;
					badger_trap_init(tsk->mm);
				}
			}
		}
	}

	return 0;
}

/*
 * This function checks whether a process name provided matches from the list
 * of process names stored to be marked for badger trap.
 */
int is_badger_trap_process(const char* proc_name)
{
	unsigned int i;
	for(i=0; i<CONFIG_NR_CPUS; i++)
	{
		if(!strncmp(proc_name,badger_trap_process[i],MAX_NAME_LEN))
			return 1;
	}
	return 0;
}

/*
 * Helper functions to manipulate all the TLB entries for reservation.
 */
inline pte_t pte_mkreserve(pte_t pte)
{
        return pte_set_flags(pte, PTE_RESERVED_MASK);
}

inline pte_t pte_unreserve(pte_t pte)
{
        return pte_clear_flags(pte, PTE_RESERVED_MASK);
}

inline int is_pte_reserved(pte_t pte)
{
        if(native_pte_val(pte) & PTE_RESERVED_MASK)
                return 1;
        else
                return 0;
}

inline pmd_t pmd_mkreserve(pmd_t pmd)
{
        return pmd_set_flags(pmd, PTE_RESERVED_MASK);
}

inline pmd_t pmd_unreserve(pmd_t pmd)
{
        return pmd_clear_flags(pmd, PTE_RESERVED_MASK);
}

inline int is_pmd_reserved(pmd_t pmd)
{
        if(native_pmd_val(pmd) & PTE_RESERVED_MASK)
                return 1;
        else
                return 0;
}

/*
 * This function walks the page table of the process being marked for badger trap
 * This helps in finding all the PTEs that are to be marked as reserved. This is
 * espicially useful to start badger trap on the fly using (2) and (3). If we do not
 * call this function, when starting badger trap for any process, we may miss some TLB
 * misses from being tracked which may not be desierable.
 *
 * Note: This function takes care of transparent hugepages and hugepages in general.
 */
void badger_trap_init(struct mm_struct *mm)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t *page_table;
	spinlock_t *ptl;
	unsigned long address;
	unsigned long i,j,k,l;
	unsigned long user = 0;
	unsigned long mask = _PAGE_USER | _PAGE_PRESENT;
	struct vm_area_struct *vma;
	pgd_t *base = mm->pgd;
	for(i=0; i<PTRS_PER_PGD; i++)
	{
		pgd = base + i;
		if((pgd_flags(*pgd) & mask) != mask)
			continue;
		for(j=0; j<PTRS_PER_PUD; j++)
		{
			pud = (pud_t *)pgd_page_vaddr(*pgd) + j;
			if((pud_flags(*pud) & mask) != mask)
                        	continue;
			address = (i<<PGDIR_SHIFT) + (j<<PUD_SHIFT);
			if(vma && pud_huge(*pud) && is_vm_hugetlb_page(vma))
			{
				spin_lock(&mm->page_table_lock);
				page_table = huge_pte_offset(mm, address);
				*page_table = pte_mkreserve(*page_table);
				spin_unlock(&mm->page_table_lock);
				continue;
			}
			for(k=0; k<PTRS_PER_PMD; k++)
			{
				pmd = (pmd_t *)pud_page_vaddr(*pud) + k;
				if((pmd_flags(*pmd) & mask) != mask)
					continue;
				address = (i<<PGDIR_SHIFT) + (j<<PUD_SHIFT) + (k<<PMD_SHIFT);
				vma = find_vma(mm, address);
				if(vma && pmd_huge(*pmd) && (transparent_hugepage_enabled(vma)||is_vm_hugetlb_page(vma)))
				{
					spin_lock(&mm->page_table_lock);
					*pmd = pmd_mkreserve(*pmd);
					spin_unlock(&mm->page_table_lock);
					continue;
				}
				for(l=0; l<PTRS_PER_PTE; l++)
				{
					pte = (pte_t *)pmd_page_vaddr(*pmd) + l;
					if((pte_flags(*pte) & mask) != mask)
						continue;
					address = (i<<PGDIR_SHIFT) + (j<<PUD_SHIFT) + (k<<PMD_SHIFT) + (l<<PAGE_SHIFT);
					vma = find_vma(mm, address);
					if(vma)
					{
						page_table = pte_offset_map_lock(mm, pmd, address, &ptl);
						*pte = pte_mkreserve(*pte);
						pte_unmap_unlock(page_table, ptl);
					}
					user++;
				}
			}
		}
	}
}
