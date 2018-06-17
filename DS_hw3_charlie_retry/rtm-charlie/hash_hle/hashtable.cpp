#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>
#include "hashtable.h"

static void resize_hash_table_if_needed(HashTable *ht);
static int hash_str(char *key);

volatile int mutex_val;

void hle_init(volatile int *val)
{
	*val = 0;
}

int hle_lock(volatile int *mutex_val)
{
	while (__atomic_exchange_n(mutex_val, 1, __ATOMIC_ACQUIRE | __ATOMIC_HLE_ACQUIRE))
		_mm_pause();
	return true;
}

int hle_release(volatile int *mutex_val)
{
	__atomic_store_n(mutex_val, 0, __ATOMIC_RELEASE | __ATOMIC_HLE_RELEASE);
	return true;
}

int hash_init(HashTable *ht)
{
	ht->size 		= HASH_TABLE_INIT_SIZE;
	ht->elem_num 	= 0;
	ht->headers		= (Header *)calloc(ht->size, sizeof(Header));

	if(ht->headers == NULL) return FAILED;
	int i;
	for(i = 0; i < ht->size; i++) {
		hle_init(&ht->headers[i].mutex_val);
	}
	LOG_MSG("[init]\tsize: %i\n", ht->size);

	return SUCCESS;
}

int hash_lookup(HashTable *ht, char *key, void **result)
{
	unsigned status;
	int index = HASH_INDEX(ht, key);
	Header *header = &(ht->headers[index]);
	hle_lock(&header->mutex_val);
	Bucket *bucket = header->next;
	if(bucket == NULL) {
		hle_release(&header->mutex_val);
		goto failed;
	}
	while(bucket)
	{
		if(strcmp(bucket->key, key) == 0)
		{
			//			LOG_MSG("[lookup]\t found %s\tindex:%i value: %p\n",
			//					key, index, bucket->value);
			*result = bucket->value;	
			return SUCCESS;
		}

		bucket = bucket->next;
	}
	hle_release(&header->mutex_val);

failed:
	LOG_MSG("[lookup]\t key:%s\tfailed\t\n", key);
	return FAILED;
}

int hash_insert(HashTable *ht,char *key, void *value)
{
	// check if we need to resize the hashtable
	unsigned status;
	Bucket *bucket = (Bucket *)malloc(sizeof(Bucket));

	resize_hash_table_if_needed(ht);

	int index = HASH_INDEX(ht, key);
	Header *header = &(ht->headers[index]);
	hle_lock(&header->mutex_val);
	Bucket *org_bucket = header->next;
	Bucket *tmp_bucket = org_bucket;
	// check if the key exits already
	while(tmp_bucket)
	{
		if(strcmp(key, tmp_bucket->key) == 0)
		{
			tmp_bucket->value = value;
			free(bucket);
			hle_release(&header->mutex_val);
			return SUCCESS;
		}

		tmp_bucket = tmp_bucket->next;
	}
	bucket->key	  = key;
	bucket->value = value;
	bucket->next  = NULL;

	ht->elem_num += 1;

	if(org_bucket != NULL)
	{
		bucket->next = org_bucket;
	}

	ht->headers[index].next= bucket;
	hle_release(&header->mutex_val);
	return SUCCESS;
}

int hash_remove(HashTable *ht, char *key)
{
	int index = HASH_INDEX(ht, key);
	Header *header = &(ht->headers[index]);
	Bucket *bucket  = header->next;
	Bucket *prev	= NULL;

	if(bucket == NULL) return FAILED;

	// find the right bucket from the link list 
	while(bucket)
	{
		if(strcmp(bucket->key, key) == 0)
		{
			LOG_MSG("[remove]\tkey:(%s) index: %d\n", key, index);

			if(prev == NULL)
			{
				ht->headers[index].next = bucket->next;
			}
			else
			{
				prev->next = bucket->next;
			}
			free(bucket);

			return SUCCESS;
		}

		prev   = bucket;
		bucket = bucket->next;
	}

	LOG_MSG("[remove]\t key:%s not found remove \tfailed\t\n", key);
	return FAILED;
}

int hash_destroy(HashTable *ht)
{
	int i;
	Bucket *cur = NULL;
	Bucket *tmp = NULL;

	for(i=0; i < ht->size; ++i)
	{
		cur = ht->headers[i].next;
		while(cur)
		{
			tmp = cur;
			cur = cur->next;
			free(tmp);
		}
	}
	free(ht->headers);

	return SUCCESS;
}

static int hash_str(char *key)
{
	int hash = 0;

	char *cur = key;

	while(*cur != '\0')
	{
		hash +=	*cur;
		++cur;
	}

	return hash;
}

static int hash_resize(HashTable *ht)
{
	// double the size
	int org_size = ht->size;
	ht->size = ht->size * 2;
	ht->elem_num = 0;

	LOG_MSG("[resize]\torg size: %i\tnew size: %i\n", org_size, ht->size);

	//	Bucket **buckets = (Bucket **)calloc(ht->size, sizeof(Bucket **));
	Header *headers = (Header *)calloc(ht->size, sizeof(Header));

	Header *org_headers = ht->headers;
	ht->headers = headers;

	int i = 0;
	for(i=0; i < org_size; ++i)
	{
		Bucket *cur = org_headers[i].next;
		Bucket *tmp;
		while(cur) 
		{
			// rehash: insert again
			hash_insert(ht, cur->key, cur->value);

			// free the org bucket, but not the element
			tmp = cur;
			cur = cur->next;
			free(tmp);
		}
	}
	free(org_headers);

	LOG_MSG("[resize] done\n");

	return SUCCESS;
}

// if the elem_num is almost as large as the capacity of the hashtable
// we need to resize the hashtable to contain enough elements
static void resize_hash_table_if_needed(HashTable *ht)
{
	if(ht->size - ht->elem_num < 1)
	{
		hash_resize(ht);	
	}
}
