#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "ctree.h"
#include "stack_linkedlist.h"

#define COMBINE_NUM	1
#define ROUND		10240000

int thread_num;
struct CombiningTree* ctree_2;
struct CombiningTree* ctree_4;
struct CombiningTree* ctree_8;
struct CombiningTree* ctree_16;
struct CombiningTree* ctree_32;
struct CombiningTree* ctree_64;

void init_root_node(struct CombiningTree* ctree)
{
	ctree->nodes[0] = malloc(sizeof(struct Node));
	struct Node* node = ctree->nodes[0];
	node->cstatus = ROOT;
	node->parent = NULL;
	node->first_val = 0;
	node->second_val = 0;
	node->result = 0;
	node->node_lock = malloc(sizeof(*(node->node_lock)));
	node->cond = malloc(sizeof(*(node->cond)));
	node->cond_result = malloc(sizeof(*(node->cond_result)));
	pthread_mutex_init(node->node_lock, NULL);
	pthread_cond_init(node->cond, NULL);
	pthread_cond_init(node->cond_result, NULL);
}

void init_node(struct CombiningTree* ctree, int i)
{
	ctree->nodes[i] = malloc(sizeof(struct Node));
	struct Node* node = ctree->nodes[i];
	struct Node* my_parent = ctree->nodes[(i - 1) / 2];
	node->parent = my_parent;
	node->cstatus = IDLE;
	node->locked = 0;
	node->first_val = 0;
	node->second_val = 0;
	node->result = 0;
	node->node_lock = malloc(sizeof(*(node->node_lock)));
	node->cond = malloc(sizeof(*(node->cond)));
	node->cond_result = malloc(sizeof(*(node->cond_result)));
	pthread_mutex_init(node->node_lock, NULL);
	pthread_cond_init(node->cond, NULL);
	pthread_cond_init(node->cond_result, NULL);
}

void init_combiningtree(struct CombiningTree* ctree, int width)
{
	int i;

	init_root_node(ctree);
	ctree->nodes[0]->id = 0;
	for (i = 1; i < width - 1; i++) {
		init_node(ctree, i);
		ctree->nodes[i]->id = i;
	}
	for (i = 0; i < (width + 1) / 2; i++) {
		ctree->leaf[(width + 1) / 2 - i - 1] = ctree->nodes[width - i - 2];
	}
}

void node_lock(struct Node* node)
{
	pthread_mutex_lock(node->node_lock);	
}

void node_unlock(struct Node* node)
{
	pthread_mutex_unlock(node->node_lock);
}

void wait(pthread_cond_t* cond, pthread_mutex_t* cond_mutex)
{
	pthread_cond_wait(cond, cond_mutex);
}

void notify_all(pthread_cond_t* cond)
{
	pthread_cond_broadcast(cond);
}

int precombine(struct Node* node)
{
	node_lock(node);
	while (node->locked == 1) {
		wait(node->cond, node->node_lock);
	}
	while (node->cstatus == RESULT) {}
	
	switch (node->cstatus) {
	case IDLE:
		node->cstatus = FIRST;
		node_unlock(node);
		return 1;
	case FIRST:
		node->locked = 1;
		node->cstatus = SECOND;
		node_unlock(node);
		return 0;
	case ROOT:
		node_unlock(node);
		return 0;
	default:
		printf("precombined unexpected Node state!\n");
		node_unlock(node);
		return 0;
	}
}

int combine(struct Node* node, int combined)
{
	node_lock(node);
	while (node->locked == 1) {
		wait(node->cond, node->node_lock);
	}
	node->locked = 1;
	node->first_val = combined;
	
	switch (node->cstatus) {
	case FIRST:
		node_unlock(node);
		return node->first_val;
	case SECOND:
		node_unlock(node);
		return node->first_val + node->second_val;
	default:
		node_unlock(node);
		return -1;
	}
}

int op(struct Node* node, int combined)
{
	node_lock(node);
	switch (node->cstatus) {
	case ROOT: {
		int prior = node->result;
		node->result += combined;
		node_unlock(node);
		return prior;
	}
	case SECOND: {
		node->second_val = combined;
		node->locked = 0;
		notify_all(node->cond);

		while (node->cstatus != RESULT) {
			wait(node->cond_result, node->node_lock);
		}

		node->cstatus = IDLE;
		node->locked = 0;
		notify_all(node->cond);
		node_unlock(node);
		return node->result;
	}

	default: {
		printf("op unexpected Node state!\n");
		node_unlock(node);
		return -1;
	}
	}
}

void distribute(struct Node* node, int prior)
{
	node_lock(node);
	switch (node->cstatus) {
	case FIRST:
		node->cstatus = IDLE;
		node->locked = 0;
		notify_all(node->cond);
		break;
	case SECOND:
		node->result = prior + node->first_val;
		node->cstatus = RESULT;
		notify_all(node->cond_result);
		break;
	default:
		printf("distribute unexpected Node state!\n");
		break;
	}
	node_unlock(node);
}

int getAndIncrement(struct CombiningTree* ctree, int id)
{
	struct Node* myleaf = ctree->leaf[id / 2];
	struct Node* node = myleaf;

	Item* top = NULL;

	while (precombine(node) == 1) {
		node = node->parent;
	}
	struct Node* stop = node;

	node = myleaf;
	int combined = COMBINE_NUM;

	while (node != stop) {
		combined = combine(node, combined);
		push(&top, node);
		node = node->parent;
	}
	int prior = op(stop, combined);
	while (isEmpty(&top) != 1) {
		struct Item* temp = pop(&top);
		if (temp->node == NULL)
			printf("NULL item node!\n");
		node = temp->node;
		distribute(node, prior);
	}
	return prior;
}


void* getAndInc_wrapper(void* ptr) 
{
	struct Args* arg = (struct Args *)ptr;
	int i;

	for (i = 0; i < ROUND / thread_num; i++) {
		if (thread_num == 2) {
			getAndIncrement(ctree_2, arg->id);
		} else if (thread_num == 4) {
			getAndIncrement(ctree_4, arg->id);
		} else if (thread_num == 8) {
			getAndIncrement(ctree_8, arg->id);
		} else if (thread_num == 16) {
			getAndIncrement(ctree_16, arg->id);
		} else if (thread_num == 32) {
			getAndIncrement(ctree_32, arg->id);
		} else if (thread_num == 64) {
			getAndIncrement(ctree_64, arg->id);
		}
	}
	return NULL;
}

int main(void) 
{
	double start_time, fin_time;

	pthread_t threads[64];
	struct Args arg[64];
	double diff;

	ctree_2 = malloc(sizeof(struct CombiningTree));
	ctree_4 = malloc(sizeof(struct CombiningTree));
	ctree_8 = malloc(sizeof(struct CombiningTree));
	ctree_16 = malloc(sizeof(struct CombiningTree));
	ctree_32 = malloc(sizeof(struct CombiningTree));
	ctree_64 = malloc(sizeof(struct CombiningTree));
	init_combiningtree(ctree_2, 2);
	init_combiningtree(ctree_4, 4);
	init_combiningtree(ctree_8, 8);
	init_combiningtree(ctree_16, 16);
	init_combiningtree(ctree_32, 32);
	init_combiningtree(ctree_64, 64);

	printf("Thread num	Time	Round	Combined_Num	Result\n");
	for (thread_num = 2; thread_num < 65; thread_num = thread_num * 2) {
		printf(" %d ", thread_num);
		
		for (int i = 0; i < thread_num; i++)
			arg[i].id = i;

		start_time = (double)clock() / CLOCKS_PER_SEC;
		
		for (int j = 0; j < thread_num; j++) {
			int ret = pthread_create(&threads[j], NULL, &getAndInc_wrapper, (void *)&arg[j]);
			if (ret)
				printf("create thread error!\n");
		}
		for (int k = 0; k < thread_num; k++) {
			int ret = pthread_join(threads[k], NULL);
			
			if (ret)
				printf("join thread error!\n");
		}

		fin_time = (double)clock() / CLOCKS_PER_SEC;
		
		diff = fin_time - start_time;

		printf("%f %d %d ", diff, ROUND, COMBINE_NUM);
		if (thread_num == 2)
			printf("%d\n", ctree_2->nodes[0]->result);
		if (thread_num == 4)
			printf("%d\n", ctree_4->nodes[0]->result);
		if (thread_num == 8)
			printf("%d\n", ctree_8->nodes[0]->result);
		if (thread_num == 16)
			printf("%d\n", ctree_16->nodes[0]->result);
		if (thread_num == 32)
			printf("%d\n", ctree_32->nodes[0]->result);
		if (thread_num == 64)
			printf("%d\n", ctree_64->nodes[0]->result);
	}
	return 0;
}
