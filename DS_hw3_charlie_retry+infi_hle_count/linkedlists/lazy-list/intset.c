/*
 * File:
 *   intset.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Linked list integer set operations
 *
 * Copyright (c) 2009-2010.
 *
 * intset.c is part of Synchrobench
 * 
 * Synchrobench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "lazy.h"

int set_contains_l(intset_l_t *set, val_t val, int transactional)
{
	// if (transactional == 2) return parse_find(set, val);
	// else return lockc_find(set, val);
	return lockc_find(set, val);
}

int set_add_l(intset_l_t *set, val_t val, int *abort_count)
{  
	// if (transactional == 2) return parse_insert(set, val);
	// else return lockc_insert(set, val);
	return lockc_insert(set, val, abort_count);
}

int set_remove_l(intset_l_t *set, val_t val, int transactional)
{
	// if (transactional == 2) return parse_delete(set, val);
	// else return lockc_delete(set, val);
	return lockc_delete(set, val);
}
