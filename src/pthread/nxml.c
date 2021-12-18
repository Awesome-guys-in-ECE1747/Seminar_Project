/*Reviewed: XML extractor*/
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>

#include "nxml.h"
#include <pthread.h>

#define MAXTHREAD 16

bool done;
int waiting;
pthread_t *thread_handler;
int thread_count = 16;
// pthread_cond_t *queue_avaliable;
// pthread_cond_t *queue_finishtask;
pthread_cond_t empty;
pthread_mutex_t mutex1;
pthread_mutex_t mutex3;

// #define DEBUG
#ifdef DEBUG
#include <assert.h>
#define WHOAMI() fprintf(stderr, "%d %s\n", __LINE__, __func__);
#define DPRINT(...) fprintf(stderr, __VA_ARGS__);
#else
#define assert(...)
#define WHOAMI()
#define DPRINT(...)
#endif

static int is_namechar(int c)
{
	return isalnum((unsigned char)c) || strchr(".-_:", c);
}
static int is_namestart(int c)
{
	return isalpha((unsigned char)c) || strchr("_:", c);
}
static int is_space(int c)
{
	return isspace((unsigned char)c);
}
static int is_quot(int c)
{
	return ('\'' == c || '\"' == c) ? c : 0; // sic!
}

static char *trim(char *str)
{
	char *d, *s = str;
	int ws = 0;

	while (is_space(*s))
		++s;
	for (d = str; *s; ++s)
	{
		if (is_space(*s))
		{
			if (!ws)
			{
				*d++ = ' ';
				ws = 1;
			}
		}
		else
		{
			ws = 0;
			*d++ = *s;
		}
	}
	if (ws)
		--d;
	*d = '\0';
	return str;
}

typedef struct Node {
    threadCtx data;
    struct Node* next;
}QNode;
 
typedef struct {
    QNode* front; 
    QNode* rear; 
}Queue;
 
Queue* CreateQueue() {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    if (!q) {
        printf("Not enough space!\n");
        return NULL;
    }
    q->front = NULL;
    q->rear = NULL;
    return q;
}
 
void AddQ(Queue* q, threadCtx item) {
    QNode* qNode = (QNode*)malloc(sizeof(QNode));
    if (!qNode) {
        printf("Not enough space!\n");
        return;
    }
    qNode->data = item;
    qNode->next = NULL;
    if (q->front == NULL) {
        q->front = qNode;
    }
    if (q->rear == NULL) {
        q->rear = qNode;
    }
    else {
        q->rear->next = qNode;
        q->rear = qNode;
    }
}
 
int IsEmptyQ(Queue* q){
    return (q->front == NULL);
}
 
threadCtx DeleteQ(Queue* q) {
    if (IsEmptyQ(q)) {
        printf("empty queue\n");
        return ERROR;
    }
    QNode* temp = q->front;
    threadCtx item;
    if (q->front == q->rear) { 
        q->front = NULL;
        q->rear = NULL;
    }
    else {
        q->front = q->front->next;
    }
    item = temp->data;
    free(temp);
    return item;
}
 
void PrintQueue(Queue* q) {
    if (IsEmptyQ(q)) {
        printf("empty queue\n");
        return;
    }
    printf("Print queue elements：\n");
    QNode* qNode = q->front;
    while (qNode != NULL) {
        printf("%d " , qNode->data);
        qNode = qNode->next;
    }
    printf("\n");
}


void en_queue(Queue* q,threadCtx content){
	// thread_queue
	pthread_mutex_lock(&mutex3);
	AddQ(q, content);
	pthread_mutex_unlock(&mutex3);
	pthread_cond_signal(&empty);
}

threadCtx de_queue(void *arg){
	Queue *Q = (Queue *)arg;
	pthread_mutex_lock(&mutex1);
	while (Q->isEmpty() && !done)
	{
		waiting++;
		if (waiting == thread_count)
		{
			done = true;
			pthread_cond_broadcast(&empty);
		}
		else
		{
			pthread_cond_wait(&empty, &mutex1);
			waiting--;
		}
	}
	threadCtx *p;
	if (!done)
		p = DeleteQ(Q);
	pthread_mutex_unlock(&mutex1);
	if (done)
		return NULL;
	else
		return p;
}

static inline char *parseAttrib(char *p, nxmlNode_t *node)
{
	char *m = p;
	char *ns, *ne, *vs, *ve;
	int quot = 0;

	while (1)
	{
		while (is_space(*m))
			++m;
		if (!is_namestart(*m))
			break;
		ns = m;
		while (is_namechar(*m))
			++m;
		ne = m;
		while (is_space(*m))
			++m;
		if ('=' != *m)
			break;
		++m;
		*ne = '\0';
		while (is_space(*m))
			++m;
		if (0 == (quot = is_quot(*m)))
			break;
		++m;
		vs = m;
		while (*m && *m != quot)
			++m;
		ve = m;
		if (*m != quot)
			break;
		++m;
		*ve = '\0';
		if (node->att_num >= node->att_sz)
		{
			void *p;
			p = realloc(node->att, sizeof *node->att * (node->att_sz + 50));
			if (!p)
				break;
			node->att = p;
			node->att_sz += 50;
		}
		node->att[node->att_num].name = ns;
		node->att[node->att_num].val = vs;
		++node->att_num;
	}
	return m;
}

static inline char *parseMarkup(char *p, nxmlNode_t *node)
{
	int i;
	char *m = p;
	char *e = m;
	static struct
	{
		const char *s;
		size_t sl;
		const char *e;
		size_t el;
		nxmlTagtype_t t;
	} stag[] = {
		{"!--", 3, "-->", 3, NXML_TYPE_COMMENT},	// comment
		{"![CDATA[", 8, "]]>", 3, NXML_TYPE_CDATA}, // cdata section
		{"?", 1, "?>", 2, NXML_TYPE_PROC},			// prolog / processing instruction
		{"!DOCTYPE", 8, ">", 1, NXML_TYPE_DOCTYPE}, // doctype definition
		{NULL, 0, NULL, 0, NXML_TYPE_EMPTY},
	};

	node->type = NXML_TYPE_EMPTY;

	if (is_namestart(*m)) // match parent/self tag
	{
		node->type = NXML_TYPE_PARENT; // tentative!
		node->name = m;
		while (is_namechar(*m))
			++m;
		e = m;
		m = parseAttrib(m, node);
		while (*m && '>' != *m)
		{ // skip any broken attribute garbage!
			++m;
		}
		if ('/' == *(m - 1))
			node->type = NXML_TYPE_SELF;
	}
	else if ('/' == *m) // match end tag
	{
		++m;
		if (is_namechar(*m))
			node->type = NXML_TYPE_END;
		node->name = m;
		while (is_namechar(*m))
			++m;
		e = m;
		while (is_space(*m))
			++m;
	}
	else // match special tags
	{
		for (i = 0; stag[i].s; ++i)
		{
			if (0 == strncasecmp(m, stag[i].s, stag[i].sl))
			{
				m += stag[i].sl;
				node->name = m;
				if (NULL != (e = strstr(m, stag[i].e)))
				{
					node->type = stag[i].t;
					m = e;
				}
				else
					e = m;
				break;
			}
		}
	}

	while (*m && '>' != *m++)
		;
	*e = '\0';
	return m;
}




// internal parser states
enum state
{
	ST_BEGIN = 0, //File Reading Begin = 0
	ST_CONTENT,	  //File Reading Content = 1
	ST_MARKUP,	  //File processing = 2
	ST_END,		  // Reach the end of the file = 3
	ST_STOP,
};

typedef struct
{
	char *p;
	char *m;
	nxmlNode_t node;
	int res;
	nxmlCb_t cb;
	void *usr;
	enum state state;
	nxmlEvent_t evt;
} threadCtx;

// Content Thread
threadCtx nxmlContent(Queue* Q, char *p, char *m, nxmlNode_t node, int res, nxmlCb_t cb, void *usr, enum state state)
{

	m = strchr(p, '<');
	if (m)
		*m++ = '\0';
	trim(p);
	if (*p)
	{
		node.type = NXML_TYPE_CONTENT;
		node.name = p;
		// res = cb(NXML_EVT_TEXT, &node, usr);
		// pthread_create(thread_handler, NULL, (void *)cb, usr);
		
		threadCtx content1 = {p, m, node, res, cb, usr, state,NXML_EVT_TEXT};
		en_queue(Q, content1);
	}
	state = m ? ST_MARKUP : ST_END;
	// DPRINT("INNER RESULT\np=%s\nm=%s\nres=%d\nstate=%d\nnodename=%s\n", p, m, res, state, node.name);
	threadCtx content = {p, m, node, res, cb, usr, state,NXML_EVT_TEXT};
	return content;
}

// Markup Thread
threadCtx nxmlMarkup(Queue* Q,char *p, char *m, nxmlNode_t node, int res, nxmlCb_t cb, void *usr, enum state state)
{
	m = parseMarkup(p, &node);
	if (NXML_TYPE_EMPTY != node.type)
	{
		if (NXML_TYPE_END != node.type)
			// res = cb(NXML_EVT_OPEN, &node, usr);
			// pthread_create(thread_handler, NULL, (void *)cb, usr);
			threadCtx content1 = {p, m, node, res, cb, usr, state,NXML_EVT_OPEN};
			en_queue(Q, content1);
		node.att_num = 0;
		if (0 == res && NXML_TYPE_PARENT != node.type)
			// res = cb(NXML_EVT_CLOSE, &node, usr);
			// pthread_create(thread_handler, NULL, (void *)cb, usr);
			threadCtx content2 = {p, m, node, res, cb, usr, state,NXML_EVT_CLOSE};
			en_queue(Q, content2);
	}
	state = ST_CONTENT;
	// DPRINT("INNER RESULT\np=%s\nm=%s\nres=%d\nstate=%d\nnodename=%s\n", p, m, res, state, node.name);
	threadCtx content = {p, m, node, res, cb, usr, state};
	return content;
}

int nxmlParse(char *buf, nxmlCb_t cb, void *usr)
{
	pthread_mutex_init(&mutex1, NULL);
  	pthread_mutex_init(&mutex3, NULL);
	Queue *Q=CreateQueue();
	int res = 0;
	char *p = buf, *m = buf;
	enum state state = ST_BEGIN;
	nxmlNode_t node;
	memset(&node, 0, sizeof node);
	threadCtx tCtx = {p, m, node, res, cb, usr, state};
	while (ST_STOP != state)
	{
		p = m;
		node.name = "";
		node.att_num = 0;
		node.error = 0;
		
		switch (state)
		{
		case ST_BEGIN:
			DPRINT("\n---------BEGIN: Current Buf ---------\n%s\n -------------------------------- \n", p);
			node.type = NXML_TYPE_EMPTY;
			// res = cb(NXML_EVT_BEGIN, &node, usr);
			// pthread_create(thread_handler, NULL, (void *)cb, usr);
			threadCtx content = {p, m, node, res, cb, usr, state,NXML_EVT_BEGIN};
			en_queue(Q, content);
			
			state = ST_CONTENT;
			break;
		case ST_END:
			DPRINT("\n---------END: Current Buf ---------\n%s\n -------------------------------- \n", p);
			node.type = NXML_TYPE_EMPTY;
			// res = cb(NXML_EVT_END, &node, usr);
			// pthread_create(thread_handler, NULL, (void *)cb, usr);
			threadCtx content = {p, m, node, res, cb, usr, state,NXML_EVT_END};
			en_queue(Q, content);
			state = ST_STOP;
			break;
		case ST_CONTENT:
			// DPRINT("\n---------CONTENT: Current Buf ---------\n%s\n -------------------------------- \n", p);
			// DPRINT("=====OUTER CONTENT=====\n");
			// m = strchr(p, '<');
			// if (m)
			// 	*m++ = '\0';
			// trim(p);
			// if (*p)
			// {
			// 	node.type = NXML_TYPE_CONTENT;
			// 	node.name = p;
			// 	res = cb(NXML_EVT_TEXT, &node, usr);
			// }
			// state = m ? ST_MARKUP : ST_END;
			;
			// threadCtx content;
			tCtx = nxmlContent(p, m, node, res, cb, usr, state);
			
			p = tCtx.p;
			m = tCtx.m;
			node = tCtx.node;
			res = tCtx.res;
			cb = tCtx.cb;
			usr = tCtx.usr;
			state = tCtx.state;

			// DPRINT("OUTER RESULT\np=%s\nm=%s\nres=%d\nstate=%d\nnodename=%s\n", p, m, res, state, node.name);
			break;
		case ST_MARKUP:
			// DPRINT("\n---------MARKUP: Current Buf ---------\n%s\n -------------------------------- \n", p);
			// m = parseMarkup(p, &node);
			// if (NXML_TYPE_EMPTY != node.type)
			// {
			// 	if (NXML_TYPE_END != node.type)
			// 		res = cb(NXML_EVT_OPEN, &node, usr);
			// 	node.att_num = 0;
			// 	if (0 == res && NXML_TYPE_PARENT != node.type)
			// 		res = cb(NXML_EVT_CLOSE, &node, usr);
			// }
			// state = ST_CONTENT;
			// DPRINT("=====OUTER CONTENT=====\n");
			;
			// threadCtx markup;
			tCtx = nxmlMarkup(p, m, node, res, cb, usr, state);

			p = tCtx.p;
			m = tCtx.m;
			node = tCtx.node;
			res = tCtx.res;
			cb = tCtx.cb;
			usr = tCtx.usr;
			state = tCtx.state;
			// DPRINT("OUTER RESULT\np=%s\nm=%s\nres=%d\nstate=%d\nnodename=%s\n", p, m, res, state, node.name);
			break;
		case ST_STOP: /* no break */
			DPRINT("\n---------STOP: Current Buf ---------\n%s\n -------------------------------- \n", p);
		default:
			// never reached!
			assert(0 == 1);
			break;
		}
		if (res)
			state = ST_STOP;
	}
	// p = markup.p;
	// m = markup.m;
	// node = markup.node;
	// res = markup.res;
	// cb = markup.cb;
	// usr = markup.usr;
	// state = markup.state;
	int thread;
  	thread_handler = (pthread_t *)malloc(thread_count * sizeof(pthread_t));
	for (thread = 0; thread < thread_count; ++thread)
	{
		pthread_create(&thread_handles[thread], NULL, cb, &Q);
	}
	for (thread = 0; thread < thread_count; thread++)
	{
		pthread_join(thread_handles[thread], NULL);
	}
	pthread_mutex_destroy(&mutex1);
	pthread_mutex_destroy(&mutex3);
	return res;
}

/* EOF */
