#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>
#include "hashtable.h"

static void resize_hash_table_if_needed(HashTable *ht);
static int hash_str(char *key);

int hash_init(HashTable *ht)
{
	ht->size 		= HASH_TABLE_INIT_SIZE;
	ht->elem_num 	= 0;
	ht->headers		= (Header *)calloc(ht->size, sizeof(Header));

	if(ht->headers == NULL) return FAILED;
	int i;
	for(i = 0; i < ht->size; i++) {
		pthread_mutex_init(&(ht->headers[i].mutex), NULL);
	}

	LOG_MSG("[init]\tsize: %i\n", ht->size);

	return SUCCESS;
}

int hash_lookup(HashTable *ht, char *key, void **result)
{
	unsigned status;
	int index = HASH_INDEX(ht, key);
	if((status = _xbegin()) == _XBEGIN_STARTED) {
		Header *header = &(ht->headers[index]);
		Bucket *bucket = header->next;
		if(bucket == NULL) {
			_xend();
			goto failed;
		}
		while(bucket)
		{
			if(strcmp(bucket->key, key) == 0)
			{
				//			LOG_MSG("[lookup]\t found %s\tindex:%i value: %p\n",
				//					key, index, bucket->value);
				*result = bucket->value;	
				_xend();
				return SUCCESS;
			}

			bucket = bucket->next;
		}
		_xend();
	}else {
		Header *header = &(ht->headers[index]);
		pthread_mutex_lock(&header->mutex);
		Bucket *bucket = header->next;
		if(bucket == NULL) {
			pthread_mutex_unlock(&header->mutex);
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
		pthread_mutex_unlock(&header->mutex);
	}
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

	if((status = _xbegin()) == _XBEGIN_STARTED) {
		Header *header = &(ht->headers[index]);
		Bucket *org_bucket = header->next;
		Bucket *tmp_bucket = org_bucket;
		// check if the key exits already
		while(tmp_bucket)
		{
			if(strcmp(key, tmp_bucket->key) == 0)
			{
				tmp_bucket->value = value;
				_xend();
				free(bucket);
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

		(ht->headers[index]).next= bucket;
		_xend();
		printf("committed\n");
	}else {
		//slowpath
		printf("abort status is %u\n", status);
		Header *header = &(ht->headers[index]);
		pthread_mutex_lock(&header->mutex);
		Bucket *org_bucket = header->next;
		Bucket *tmp_bucket = org_bucket;
		// check if the key exits already
		while(tmp_bucket)
		{
			if(strcmp(key, tmp_bucket->key) == 0)
			{
				tmp_bucket->value = value;
				free(bucket);
				pthread_mutex_unlock(&header->mutex);
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
		pthread_mutex_unlock(&header->mutex);

	}

	return SUCCESS;
}

int hash_remove(HashTable *ht, char *key)
{
	unsigned status;
	int index = HASH_INDEX(ht, key);
	if((status = _xbegin()) == _XBEGIN_STARTED) {
		Header *header = &(ht->headers[index]);
		Bucket *bucket  = header->next;
		Bucket *prev	= NULL;
		if(bucket == NULL) {
			_xend();
			return FAILED;
		}

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
				_xend();
				free(bucket);
				return SUCCESS;
			}
			prev   = bucket;
			bucket = bucket->next;
		}
		_xend();
	}else {
		Header *header = &(ht->headers[index]);
		pthread_mutex_lock(&header->mutex);
		Bucket *bucket  = header->next;
		Bucket *prev	= NULL;
		if(bucket == NULL) {
			pthread_mutex_unlock(&header->mutex);
			return FAILED;
		}

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
				pthread_mutex_unlock(&header->mutex);
				free(bucket);
				return SUCCESS;
			}
			prev   = bucket;
			bucket = bucket->next;
		}
		pthread_mutex_unlock(&header->mutex);
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
