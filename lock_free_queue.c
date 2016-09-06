#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef enum tagNodeStatus {
	NODE_STATUS_FREE = 0x0,
	NODE_STATUS_ALLOCATE,
	NODE_STATUS_ACTIVATE,
	NODE_STATUS_DEACTIVATE,
} NodeStatus;

typedef enum tagQueueStatus {
	QUEUE_STATUS_SUCCESS = 0x0,
	QUEUE_STATUS_FULL = 0x1,
	QUEUE_STATUS_EMPTY = 0x2,
} QueueStatus;

typedef struct tagQnode {
	struct tagQnode *next;
	NodeStatus status;
	void* data;
} Qnode;

typedef struct tagLockFreeQueue {
	Qnode *nodePool;
	Qnode *head;
	Qnode *tail;
	uint32_t nextIndex;
	uint32_t poolHead;
	uint32_t poolTail;
	uint32_t capacity;
} LockFreeQueue;

LockFreeQueue* H_CreateLockFreeQueue(uint32_t capacity) 
{
	LockFreeQueue *q = (LockFreeQueue*)malloc(sizeof(LockFreeQueue));
	if (q) {
		q->nodePool = (Qnode*)malloc(sizeof(Qnode) * capacity);
		memset(q->nodePool, 0, sizeof(Qnode) * capacity);
		if (q->nodePool) {
			q->nodePool[0].data = NULL;
			q->nodePool[0].next = NULL;
			q->nodePool[0].status = NODE_STATUS_ALLOCATE;
			q->nextIndex = 1;
			q->head = q->nodePool;
			q->tail = q->nodePool;
			q->capacity = capacity;
			return q;
		}
		else {
			free(q);
			return NULL;
		}
	} 
	else {
		return NULL;
	}
}

Qnode* H_GetFreeNode(LockFreeQueue *q)
{
	if (__sync_bool_compare_and_swap(&q->nodePool[q->nextIndex].status, NODE_STATUS_FREE, NODE_STATUS_ALLOCATE)) {
		Qnode *temp = &q->nodePool[q->nextIndex];
		q->nextIndex = (q->nextIndex + 1) % q->capacity;
		return temp;
	}
	else {
		return NULL;
	}
}

QueueStatus H_WriteQueue(LockFreeQueue *q, void *data)
{
	Qnode *node = H_GetFreeNode(q);
	if (node) {
		Qnode *pre = __sync_lock_test_and_set(&q->tail, node);
		pre->next = node;
		pre->data = data;
		pre->status = NODE_STATUS_ACTIVATE;
		return QUEUE_STATUS_SUCCESS;
	}
	else {
		return QUEUE_STATUS_FULL;
	}
}

QueueStatus H_ReadQueue(LockFreeQueue *q, void **data)
{
	if (__sync_bool_compare_and_swap(&q->head->status, NODE_STATUS_ACTIVATE, NODE_STATUS_DEACTIVATE)) {
		Qnode *pre = __sync_lock_test_and_set(&q->head, q->head->next);
		if (pre->status == NODE_STATUS_DEACTIVATE) {
			*data = pre->data;
			pre->status = NODE_STATUS_FREE;
		} else {
			printf("Wrong. %d\n", pre->status);
		}
		return QUEUE_STATUS_SUCCESS;
	}
	else {
		return QUEUE_STATUS_EMPTY;
	}
}

int data[100];
int sum;

void* write_thread(void* arg)
{
	LockFreeQueue* q = (LockFreeQueue*)arg;
	int i;
	for (i = 0; i < 100; i++) {
		while (H_WriteQueue(q, &data[i]) != QUEUE_STATUS_SUCCESS);
	}
	return NULL;
}

void* read_thread(void* arg)
{
	LockFreeQueue* q = (LockFreeQueue*)arg;
	int i;
	void *temp;
	for (i = 0; i < 500; i++) {
		while (H_ReadQueue(q, &temp) != QUEUE_STATUS_SUCCESS);
		__sync_fetch_and_add(&sum, *(int*)temp);
	}
	return NULL;
}

int main(int argc, char* argv[]) 
{
	int i;
	for (i = 0; i < 100; i++) data[i] = i;
	LockFreeQueue *q = H_CreateLockFreeQueue(200);
	pthread_t handle[20];
	sum = 0;
	for (i = 0; i < 10; i++) {
		pthread_create(&handle[i], NULL, write_thread, (void*)q);
	}

	for (i = 10; i < 12; i++) {
		pthread_create(&handle[i], NULL, read_thread, (void*)q);
	}
	for (i = 0; i < 12; i++) {
		pthread_join(handle[i], NULL);
	}
	printf("%d\n", sum);
	return 0;
}
