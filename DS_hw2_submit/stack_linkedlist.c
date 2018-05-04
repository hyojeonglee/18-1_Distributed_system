#include <stdio.h>
#include <stdlib.h>
#include "stack_linkedlist.h"

void push(Item** top, struct Node* node)
{
	Item* temp = (Item*)malloc(sizeof(Item));
	temp->node = node;
       	temp->next = *top;
	*top = temp;
}

struct Item* pop(Item** top)
{
	Item* temp = *top;
	Item* res = *top;
	if (*top == NULL) return NULL;
	*top = temp->next;
	return res;
}

int isEmpty(Item** top)
{
	Item* temp = *top;
	int i = 0;
	while (temp != NULL) {
		temp = temp->next;
		i++;
	}
	if (i == 0) {
		return 1;
	}
	return 0;
}

