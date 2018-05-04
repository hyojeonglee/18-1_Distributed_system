#define INFI 256

struct Args {
	int id;
};

enum Cstatus { IDLE, FIRST, SECOND, RESULT, ROOT };

struct Node {
	int locked;
	enum Cstatus cstatus;
	int first_val, second_val;
	int result;
	struct Node* parent;
	pthread_mutex_t* node_lock;
	pthread_cond_t* cond;
	pthread_cond_t* cond_result;
	int id;
};

struct CombiningTree {
	struct Node* nodes[INFI];
	struct Node* leaf[INFI];
};

/* init root node */
void init_root_node(struct CombiningTree* ctree);
/* init other nodes */
void init_node(struct CombiningTree* ctree, int i);
int precombine(struct Node* node);
int combine(struct Node* node, int combined);
int op(struct Node* node, int combined);
void distribute(struct Node* node, int prior);
void wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
void notify_all(pthread_cond_t* cond);
void node_lock(struct Node* node);
void node_unlock(struct Node* node);
/* init combining tree */
void init_combiningtree(struct CombiningTree* ctree, int width);
int getAndIncrement(struct CombiningTree* ctree, int id);
