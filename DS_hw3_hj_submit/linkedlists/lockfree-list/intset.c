/*
 *  intset.c
 *  
 *  Integer set operations (contain, insert, delete) 
 *  that call stm-based / lock-free counterparts.
 *
 *  Created by Vincent Gramoli on 1/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "intset.h"

int set_contains(intset_t *set, val_t val, int transactional)
{
	int result;

#ifdef DEBUG
	//	printf("++> set_contains(%d)\n", (int)val);
	//	IO_FLUSH;
#endif

#ifdef SEQUENTIAL
	node_t *prev, *next;

	prev = set->head;
	next = prev->next;
	while (next->val < val) {
		prev = next;
		next = prev->next;
	}
	result = (next->val == val);

#elif defined STM			
	unsigned status;
	node_t *prev, *next;
	val_t v = 0;

	if (transactional > 1) {
//added by charlie
		if((status = _xbegin()) == _XBEGIN_STARTED){
			prev = set->head;
			next = (node_t *)TX_LOAD(&prev->next);
			while (1) {
				v = TX_LOAD((uintptr_t *) &next->val);
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)TX_LOAD(&prev->next);
			}		  
			result = (v == val);
			_xend();
			//printf("contains commited\n");
		}else{
		//	printf("contains abort\n");
			result = harris_find(set, val);
		}
//charlie finish
	} else {
//		printf("set_contains no transactional\n");
		TX_START(NL);
		prev = set->head;
		next = (node_t *)TX_LOAD(&prev->next);
		while (1) {
			v = TX_LOAD((uintptr_t *) &next->val);
			if (v >= val)
				break;
			prev = next;
			next = (node_t *)TX_LOAD(&prev->next);
		}		  
		TX_END;
		result = (v == val);
	}


#elif defined LOCKFREE			
	//	result = harris_find(set, val);
#endif	

	return result;
}

inline int set_seq_add(intset_t *set, val_t val)
{
	int result;
	node_t *prev, *next;

	prev = set->head;
	next = prev->next;
	while (next->val < val) {
		prev = next;
		next = prev->next;
	}
	result = (next->val != val);
	if (result) {
		prev->next = new_node(val, next, 0);
	}
	return result;
}	


int set_add(intset_t *set, val_t val, int transactional)
{
	int result;

#ifdef DEBUG
	//	printf("++> set_add(%d)\n", (int)val);
	//	IO_FLUSH;
#endif

	if (!transactional) {

		result = set_seq_add(set, val);
	//	printf(" 1 add seqq\n");
	} else { 

#ifdef SEQUENTIAL /* Unprotected */

		result = set_seq_add(set, val);
	//	printf(" 2 add seqq\n");

#elif defined STM

		node_t *prev, *next;
		val_t v;	

		if (transactional > 2) {
//added by charlie
			unsigned status;
			node_t *n_node = new_node(val, NULL, transactional); 
			if((status = _xbegin()) == _XBEGIN_STARTED) {
				prev = set->head;
				next = (node_t *)TX_LOAD(&prev->next);
				while (1) {
					v = TX_LOAD((uintptr_t *) &next->val);
					if (v >= val)
						break;
					prev = next;
					next = (node_t *)TX_LOAD(&prev->next);
				}
				result = (v != val);
				if (result) {
					n_node->next = next;
					TX_STORE(&prev->next, n_node);
				}
				_xend();
			}else{
				//tx fail 하면 harris_insert로 
			//	printf("add abort\n");
				result = -1; //asking for retry
			}
//charie finish
		} else {
			printf("set_add no transactional\n");
			TX_START(NL);
			prev = set->head;
			next = (node_t *)TX_LOAD(&prev->next);
			while (1) {
				v = TX_LOAD((uintptr_t *) &next->val);
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)TX_LOAD(&prev->next);
			}
			result = (v != val);
			if (result) {
				TX_STORE(&prev->next, new_node(val, next, transactional));
			}
			TX_END;
		}
#elif defined LOCKFREE

		result = harris_insert(set, val);
#endif
	}

	return result;
}

int set_remove(intset_t *set, val_t val, int transactional)
{
	int result = 0;

#ifdef DEBUG
	//	printf("++> set_remove(%d)\n", (int)val);
	//	IO_FLUSH;
#endif

#ifdef SEQUENTIAL /* Unprotected */

	node_t *prev, *next;

	prev = set->head;
	next = prev->next;
	while (next->val < val) {
		prev = next;
		next = prev->next;
	}
	result = (next->val == val);
	if (result) {
		prev->next = next->next;
		free(next);
	}

#elif defined STM

	node_t *prev, *next;
	val_t v;
	node_t *n;
	unsigned status;

	if (transactional > 3) {
//added by charlie
		if((status = _xbegin()) == _XBEGIN_STARTED) {
			prev = set->head;
			next = (node_t *)TX_LOAD(&prev->next);
			while (1) {
				v = TX_LOAD((uintptr_t *) &next->val);
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)TX_LOAD(&prev->next);
			}
			result = (v == val);
			if (result) {
				n = (node_t *)TX_LOAD(&next->next);
				TX_STORE(&prev->next, n);
			}
			_xend();
	//		printf("remove commited\n");
			if(result)
				FREE(next, sizeof(node_t));
//charlie finish
		}else{
	//		printf("remove abort\n");
			result = harris_delete(set, val);
		}

	} else {
		TX_START(NL);
		prev = set->head;
		next = (node_t *)TX_LOAD(&prev->next);
		while (1) {
			v = TX_LOAD((uintptr_t *) &next->val);
			if (v >= val)
				break;
			prev = next;
			next = (node_t *)TX_LOAD(&prev->next);
		}
		result = (v == val);
		if (result) {
			n = (node_t *)TX_LOAD(&next->next);
			TX_STORE(&prev->next, n);
			FREE(next, sizeof(node_t));
		}
		TX_END;
	}

#elif defined LOCKFREE
	//	result = harris_delete(set, val);
#endif

	return result;
}


