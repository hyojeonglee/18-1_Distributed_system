typedef struct Item {
	struct Node* node;
	struct Item* next;
} Item;

void push(Item** top, struct Node* node);
struct Item* pop(Item** top);
int isEmpty(Item** top);
