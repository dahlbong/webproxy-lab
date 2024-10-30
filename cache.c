#include <stdio.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct CacheNode {
    char *path, *data;      // key: value = path: data
    size_t size;            // 캐시된 데이터 크기
    time_t timestamp;       // 캐시된 시간 (유효성 검사)
    struct CacheNode *next; // 연결리스트 다음 노드 포인터
} CacheNode;

typedef struct Cache {
    CacheNode *head;        // 시작 노드
    size_t curSize;         // 연결리스트 사이즈
} Cache;


Cache *createCache();
void freeCache(Cache *cache);

CacheNode *createCacheNode(const char *path, const char *data, size_t size);
void evictOldCache(Cache *cache);

CacheNode *readCache(Cache *cache, const char *path);
void writeCache(Cache *cache, const char *path, const char *data, size_t size);
void sendCache(Cache *cache, int clientfd, CacheNode *node);


/* [CREATE] CacheNode 생성 */
CacheNode *createCacheNode(const char *path, const char *data, size_t size) {
    CacheNode *node = malloc(sizeof(CacheNode));
    node->path = strdup(path);      // 파라미터 문자열 복사하여 새로 할당된 메모리에 저장
    node->data = malloc(size);
    memcpy(node->data, data, size);
    node->size = size;
    node->next = NULL;
    return node;
}

/* [READ-1] CacheNode 탐색 */
CacheNode* readCache(Cache *cache, const char *path) {
    CacheNode *cur = cache->head;
    while (cur) {
        if (strcmp(cur->path, path) == 0) return cur;
        cur = cur->next;
    }
    return NULL; // 캐시에 데이터 없음 -> write 수행
}

/* [READ-2] proxy가 Cache에서 찾으면 데이터 보내고, 그 데이터가 있는 CacheNode는 가장 마지막 노드로 보내기(LRU) */
void sendCache(Cache *cache, int clientfd, CacheNode *node) {
    /* 캐시된 데이터 클라이언트에 전송 */
    Rio_writen(clientfd, node->data, node->size);
    printf("Sent cached data for path: %s\n", node->path);
    if (cache->head == node) return;
    
    /* 순번 update: 노드가 head가 아니면 send하는 노드를 head로 만들기 */
    CacheNode *prev = cache->head;
    while (prev->next != node) prev = prev->next;   // 이전 노드 찾기
    prev->next = node->next;
    node->next = cache->head;
    cache->head = node;
}

/* [UPDATE] proxy가 Cache에 새로운 데이터 추가하려고 하면, 가장 마지막 노드에 새 cacheNode 추가하기  */
void writeCache(Cache *cache, const char *path, const char *data, size_t size) {
    if (size > MAX_CACHE_SIZE) {
        printf("Data too large to cache.\n");
        return;
    }

    while (cache->curSize + size > MAX_CACHE_SIZE) {
        evictOldCache(cache);
    }
    
    CacheNode *newNode = createCacheNode(path, data, size);
    newNode->next = cache->head;
    cache->head = newNode;
    cache->curSize += size;
}

/* [DELETE] 가장 첫번째 cacheNode 삭제(LRU) */
void evictOldCache(Cache *cache) {
    CacheNode *evict = cache->head;
    cache->head = evict->next;
    cache->curSize -= evict->size;

    free(evict->path);
    free(evict->data);
    free(evict);
}


/* Cache 초기화 */
Cache* createCache() {
    Cache *cache = malloc(sizeof(Cache));
    cache->head = NULL;
    cache->curSize = 0;
    return cache;
}

/* Cache 구조체 해제 */
void freeCache(Cache *cache) {
    CacheNode *cur = cache->head;
    while (cur) {
        CacheNode *temp = cur;
        cur = cur->next;
        free(temp->path);
        free(temp->data);
        free(temp);
    }
    free(cache);
}
